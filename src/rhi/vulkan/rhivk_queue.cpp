#include "rhi/vulkan/rhivk_queue.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace rhi::vulkan {

RHIVKQueue::RHIVKQueue(VkQueue queue, VkDevice device)
    : queue_(queue), device_(device) {}

void RHIVKQueue::submit(const std::vector<rhi::CommandBuffer*>& commandBuffers, rhi::Fence* /*fence*/, rhi::Semaphore* /*waitSemaphore*/) {
    std::vector<VkCommandBuffer> vkCmds;
    for (auto* cmd : commandBuffers) {
        auto* vkCmd = static_cast<RHIVKCommandBuffer*>(cmd);
        vkCmds.push_back(vkCmd->get_vk());
    }
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = static_cast<uint32_t>(vkCmds.size());
    submitInfo.pCommandBuffers = vkCmds.data();
    vkQueueSubmit(queue_, 1, &submitInfo, VK_NULL_HANDLE);
}

void RHIVKQueue::wait_idle() {
    vkQueueWaitIdle(queue_);
}

} // namespace rhi::vulkan
