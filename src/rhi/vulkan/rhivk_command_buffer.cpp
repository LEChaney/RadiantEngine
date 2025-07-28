#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_image.h"
#include "rhi/vulkan/rhivk_image_view.h"
#include "rhi/vulkan/rhivk_buffer.h"
#include <vulkan/vulkan.h>
#include "rhivk_command_buffer.h"

namespace rhi::vulkan {


UniquePtr<RHIVKCommandBuffer> RHIVKCommandBuffer::createUnique(RHIVKContext* context) {
    return UniquePtr<RHIVKCommandBuffer>(new RHIVKCommandBuffer(context));
}

RHIVKCommandBuffer::RHIVKCommandBuffer(RHIVKContext* context)
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

RHIVKCommandBuffer::~RHIVKCommandBuffer() {
    if (m_cmdBuffer && m_context) {
        vkFreeCommandBuffers(
            m_context->getVkDevice(), 
            m_context->getVkCommandPool(), 
            1, 
            &m_cmdBuffer);
    }
}

void RHIVKCommandBuffer::begin()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(m_cmdBuffer, &beginInfo);
}

void RHIVKCommandBuffer::end() {
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

void RHIVKCommandBuffer::reset()
{
    vkResetCommandBuffer(m_cmdBuffer, 0);
}

void RHIVKCommandBuffer::clearColor(RHIImage* image, const glm::vec4& color)
{
    VkImage vkImage = static_cast<RHIVKImage*>(image)->getVk();
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

void RHIVKCommandBuffer::transitionImageLayout(RHIImage *image, RHIImageLayout newLayout)
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

void RHIVKCommandBuffer::transitionImageLayout(RHIImage* image, RHIImageLayout oldLayout, RHIImageLayout newLayout) {
    if (oldLayout == newLayout) {
        // No transition needed
        return;
    }

    VkImage vkImage = static_cast<RHIVKImage*>(image)->getVk();

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

void RHIVKCommandBuffer::copyImageToBuffer(rhi::RHIImage* image, rhi::RHIBuffer* buffer, uint32_t width, uint32_t height) {
    VkImage vkImage = static_cast<RHIVKImage*>(image)->getVk();
    VkBuffer vkBuffer = static_cast<RHIVKBuffer*>(buffer)->getVk();
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

} // namespace rhi::vulkan
