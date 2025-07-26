#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_image_view.h"
#include <vulkan/vulkan.h>

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
} // namespace rhi::vulkan
