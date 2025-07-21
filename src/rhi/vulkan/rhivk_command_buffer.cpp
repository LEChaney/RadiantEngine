#include "rhi/vulkan/rhivk_command_buffer.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

RHIVKCommandBuffer::RHIVKCommandBuffer(VkCommandBuffer cmdBuffer, VkDevice device, VkCommandPool pool)
    : cmdBuffer_(cmdBuffer), device_(device), pool_(pool) {}

void RHIVKCommandBuffer::begin() {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmdBuffer_, &beginInfo);
}

void RHIVKCommandBuffer::end() {
    vkEndCommandBuffer(cmdBuffer_);
}

} // namespace rhi::vulkan
