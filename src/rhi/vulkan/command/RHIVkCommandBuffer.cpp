#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/command/RHIVkCommandBuffer.h"
#include "rhi/interface/descriptor/RHIDescriptorSet.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/buffer/RHIVkBuffer.h"
#include "rhi/vulkan/pipeline/RHIVkPipeline.h"
#include "rhi/vulkan/pipeline/RHIVkPipelineLayout.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include <limits>

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

void RHIVkCommandBuffer::copyImageToBuffer(rhi::RHIImage* image, rhi::RHIBuffer* buffer, uint32 width, uint32 height) {
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

    m_boundPipeline = pipeline;
}

void RHIVkCommandBuffer::bindDescriptorBuffers(const Array<RHIDescriptorBuffer*>& descBuffers) {
    if (descBuffers == m_boundDescriptorBuffers) {
        return; // Buffer set already bound, no need to do anything
    }

    Array<VkDescriptorBufferBindingInfoEXT> bindingInfos;
    bindingInfos.resize(descBuffers.size());
    for (int32 i = 0; i < descBuffers.size(); ++i) {
        VkBufferDeviceAddressInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferInfo.buffer = static_cast<RHIVkBuffer*>(descBuffers[i]->getBuffer())->getVk();
        VkDeviceAddress address = vkGetBufferDeviceAddress(m_context->getVkDevice(), &bufferInfo);

        bindingInfos[i].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
        bindingInfos[i].usage = toVkBufferUsageFlags(descBuffers[i]->getUsage());
        bindingInfos[i].address = address;

        // Create a mapping so we can look up a bound descriptor buffer's binding index
        m_boundDescriptorBuffersToIndex[descBuffers[i]] = i;
    }

    vkCmdBindDescriptorBuffersEXT(
        m_cmdBuffer,
        bindingInfos.size(),
        bindingInfos.data()
    );

    m_boundDescriptorBuffers = descBuffers;
}
void RHIVkCommandBuffer::bindDescriptorSets(const Array<RHIDescriptorSetBinding>& setBindings,
    RHIPipelineLayout* pipelineLayout, RHIPipelineBindPoint bindPoint)
{
    SmallArray<uint32, 8> bufferIndices;
    SmallArray<VkDeviceSize, 8> setOffsetsInBuffer;
    constexpr uint32 INVALID_SET_INDEX = std::numeric_limits<uint32>::max();
    uint32 prevSetIndex = INVALID_SET_INDEX; // Max uint32
    for (const auto& [setIndex, set] : setBindings) {
        if (prevSetIndex == INVALID_SET_INDEX) {
            prevSetIndex = setIndex - 1;
        }
        ASSERT(setIndex == prevSetIndex + 1 && "Descriptor set indices must be contiguous");
        ASSERT(set.buffer && "Descriptor buffer is NULL");
        ASSERT(m_boundDescriptorBuffersToIndex.contains(set.buffer) && "Descriptor buffer not bound");
        bufferIndices.push_back(m_boundDescriptorBuffersToIndex[set.buffer]);
        setOffsetsInBuffer.push_back(set.offset);
        prevSetIndex = setIndex;
    }

    auto vKPipelineLayout = static_cast<RHIVkPipelineLayout*>(pipelineLayout);
    vkCmdSetDescriptorBufferOffsetsEXT(
        m_cmdBuffer,
        toVkPipelineBindPoint(bindPoint),
        vKPipelineLayout->getVk(),
        setBindings[0].setIndex,
        setBindings.size(),
        bufferIndices.data(),
        setOffsetsInBuffer.data()
    );
}
void RHIVkCommandBuffer::pushConstants(RHIPipelineLayout* layout,
    RHIShaderStageFlags shaderStageFlags, uint32 offset, uint32 size, const void* data)
{
    vkCmdPushConstants(
        m_cmdBuffer,
        static_cast<RHIVkPipelineLayout*>(layout)->getVk(),
        toVkShaderStageFlags(shaderStageFlags),
        offset,
        size,
        data
    );
}

} // namespace rhi::vulkan
