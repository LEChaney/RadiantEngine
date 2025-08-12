#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/command/RHIVkCommandBuffer.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/buffer/RHIVkBuffer.h"
#include "rhi/vulkan/pipeline/RHIVkPipeline.h"
#include "rhi/vulkan/pipeline/RHIVkPipelineLayout.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi::vulkan {


UniquePtr<RHIVkCommandBuffer> RHIVkCommandBuffer::createUnique(RHIVkContext* context) {
    return UniquePtr<RHIVkCommandBuffer>(new RHIVkCommandBuffer(context));
}

RHIVkCommandBuffer::RHIVkCommandBuffer(RHIVkContext* context)
    : m_context(context) 
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = context->getVkCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(context->getVkDevice(), &allocInfo, &m_cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }
}

RHIVkCommandBuffer::~RHIVkCommandBuffer() {
    if (m_cmdBuffer && m_context) {
        vkFreeCommandBuffers(
            m_context->getVkDevice(), 
            m_context->getVkCommandPool(), 
            1, 
            &m_cmdBuffer);
    }
}

void RHIVkCommandBuffer::begin()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(m_cmdBuffer, &beginInfo);
}

void RHIVkCommandBuffer::end() {
    vkEndCommandBuffer(m_cmdBuffer);

    // After ending the command buffer, we can flush the tracked image layout states,
    // and store the last known layouts on the images.
    for (const auto& [image, layout] : m_trackedImageLayouts) {
        // TODO: Thread safety: How should this be handled? The image storage is really just convenience for tracking.
        // In a multi-threaded context, we might just not want to do this.
        image->m_lastKnownLayout = layout;
    }
    m_trackedImageLayouts.clear();
}

void RHIVkCommandBuffer::reset()
{
    vkResetCommandBuffer(m_cmdBuffer, 0);
}

void RHIVkCommandBuffer::clearColor(RHIImage* image, const glm::vec4& color)
{
    VkImage vkImage = static_cast<RHIVkImage*>(image)->getVk();
    VkClearColorValue clearColor{};
    clearColor.float32[0] = color.r;
    clearColor.float32[1] = color.g;
    clearColor.float32[2] = color.b;
    clearColor.float32[3] = color.a;
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    vkCmdClearColorImage(m_cmdBuffer, vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
}

void RHIVkCommandBuffer::transitionImageLayout(RHIImage *image, RHIImageLayout newLayout)
{
    RHIImageLayout oldLayout;
    if (m_trackedImageLayouts.contains(image)) {
        oldLayout = m_trackedImageLayouts[image];
    } else {
        // TODO: Assert if not main thread, this is not thread-safe
        oldLayout = image->m_lastKnownLayout;
    }

    transitionImageLayout(image, oldLayout, newLayout);
}

void RHIVkCommandBuffer::transitionImageLayout(RHIImage* image, RHIImageLayout oldLayout, RHIImageLayout newLayout) {
    if (oldLayout == newLayout) {
        // No transition needed
        return;
    }

    VkImage vkImage = static_cast<RHIVkImage*>(image)->getVk();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // Map RHI ImageLayout to Vulkan VkImageLayout
    auto toVkLayout = [](RHIImageLayout layout) {
        switch (layout) {
            case RHIImageLayout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
            case RHIImageLayout::General: return VK_IMAGE_LAYOUT_GENERAL;
            case RHIImageLayout::TransferSrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case RHIImageLayout::TransferDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case RHIImageLayout::ColorAttachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case RHIImageLayout::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            default: return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };
    barrier.oldLayout = toVkLayout(oldLayout);
    barrier.newLayout = toVkLayout(newLayout);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vkImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0; // For simplicity, could be improved
    barrier.dstAccessMask = 0;
    vkCmdPipelineBarrier(
        m_cmdBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_trackedImageLayouts[image] = newLayout;
}

void RHIVkCommandBuffer::copyImageToBuffer(rhi::RHIImage* image, rhi::RHIBuffer* buffer, uint32_t width, uint32_t height) {
    VkImage vkImage = static_cast<RHIVkImage*>(image)->getVk();
    VkBuffer vkBuffer = static_cast<RHIVkBuffer*>(buffer)->getVk();
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(m_cmdBuffer, vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkBuffer, 1, &region);
}

void RHIVkCommandBuffer::bindComputePipeline(RHIPipeline* pipeline) {
    auto* vkPipe = static_cast<RHIVkPipeline*>(pipeline);
    vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, vkPipe->getVk());
}

void RHIVkCommandBuffer::bindDescriptorBuffer(RHIPipelineLayout* layout, uint32_t setIndex, RHIBuffer* buffer, uint64 offset) {
    auto* vkLayout = static_cast<RHIVkPipelineLayout*>(layout);
    auto* vkBuffer = static_cast<RHIVkBuffer*>(buffer);

    // Bind the descriptor buffer(s)
    VkDescriptorBufferBindingInfoEXT bindingInfo{};
    bindingInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bindingInfo.address = 0; // will be set from device address of buffer
    bindingInfo.usage = toVkBufferUsageFlags(vkBuffer->getUsage());

    VkBufferDeviceAddressInfo addrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfo.buffer = vkBuffer->getVk();
    VkDeviceAddress deviceAddr = vkGetBufferDeviceAddress(m_context->getVkDevice(), &addrInfo);

    bindingInfo.address = deviceAddr;

    vkCmdBindDescriptorBuffersEXT(m_cmdBuffer, 1, &bindingInfo);

    // Now set the offset for the set index we want to use
    uint32_t bufferIndex = 0; // we bound one buffer at index 0
    VkDeviceSize setOffset = static_cast<VkDeviceSize>(offset);
    vkCmdSetDescriptorBufferOffsetsEXT(
        m_cmdBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        vkLayout->getVk(),
        setIndex,     // firstSet
        1,            // setCount
        &bufferIndex, // pBufferIndices
        &setOffset);  // pOffsets
}

} // namespace rhi::vulkan
