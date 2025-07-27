#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_image_view.h"
#include "rhi/vulkan/rhivk_buffer.h"
#include <vulkan/vulkan.h>
#include "rhivk_command_buffer.h"

namespace rhi::vulkan {

RHIVKCommandBuffer::RHIVKCommandBuffer(VkCommandBuffer cmd_buffer, RHIVKContext* context)
    : m_cmd_buffer(cmd_buffer), m_context(context) {}

RHIVKCommandBuffer::~RHIVKCommandBuffer() {
    if (m_cmd_buffer && m_context) {
        vkFreeCommandBuffers(
            m_context->get_vk_device(), 
            m_context->get_vk_command_pool(), 
            1, 
            &m_cmd_buffer);
    }
}

void RHIVKCommandBuffer::begin() {
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(m_cmd_buffer, &begin_info);
}

void RHIVKCommandBuffer::end() {
    vkEndCommandBuffer(m_cmd_buffer);
}

void RHIVKCommandBuffer::reset()
{
    vkResetCommandBuffer(m_cmd_buffer, 0);
}

void RHIVKCommandBuffer::clear_color(rhi::RHIImage* image, const glm::vec4& color)
{
    VkImage vk_image = static_cast<RHIVKImage*>(image)->get_vk();
    VkClearColorValue clear_color{};
    clear_color.float32[0] = color.r;
    clear_color.float32[1] = color.g;
    clear_color.float32[2] = color.b;
    clear_color.float32[3] = color.a;
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    vkCmdClearColorImage(m_cmd_buffer, vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &range);
}

void RHIVKCommandBuffer::transition_image_layout(rhi::RHIImage* image, ImageLayout oldLayout, ImageLayout newLayout) {
    VkImage vk_image = static_cast<RHIVKImage*>(image)->get_vk();
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // Map RHI ImageLayout to Vulkan VkImageLayout
    auto to_vk_layout = [](ImageLayout layout) {
        switch (layout) {
            case ImageLayout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
            case ImageLayout::General: return VK_IMAGE_LAYOUT_GENERAL;
            case ImageLayout::TransferSrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            case ImageLayout::TransferDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            case ImageLayout::ColorAttachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            case ImageLayout::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            default: return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    };
    barrier.oldLayout = to_vk_layout(oldLayout);
    barrier.newLayout = to_vk_layout(newLayout);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vk_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0; // For simplicity, could be improved
    barrier.dstAccessMask = 0;
    vkCmdPipelineBarrier(
        m_cmd_buffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void RHIVKCommandBuffer::copy_image_to_buffer(rhi::RHIImage* image, rhi::RHIBuffer* buffer, uint32_t width, uint32_t height) {
    VkImage vk_image = static_cast<RHIVKImage*>(image)->get_vk();
    VkBuffer vk_buffer = static_cast<RHIVKBuffer*>(buffer)->get_vk();
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
    vkCmdCopyImageToBuffer(m_cmd_buffer, vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk_buffer, 1, &region);
}

} // namespace rhi::vulkan
