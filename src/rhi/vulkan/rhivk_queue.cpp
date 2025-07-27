#include "rhi/vulkan/rhivk_queue.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_fence.h"
#include "rhi/vulkan/rhivk_semaphore.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace rhi::vulkan {

RHIVKQueue::RHIVKQueue(VkQueue queue, RHIVKContext* context)
    : m_queue(queue), m_context(context) {}

RHIVKQueue::~RHIVKQueue() {
    // No explicit cleanup needed, Vulkan device destruction handles queue destruction
}

void RHIVKQueue::submit(const Array<rhi::RHICommandBuffer*>& command_buffers, rhi::RHIFence* fence, rhi::RHISemaphore* wait_semaphore) {
    Array<VkCommandBuffer> vk_cmds;
    for (auto* cmd : command_buffers) {
        auto* vk_cmd = static_cast<RHIVKCommandBuffer*>(cmd);
        vk_cmds.push_back(vk_cmd->get_vk());
    }
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = static_cast<uint32_t>(vk_cmds.size());
    submit_info.pCommandBuffers = vk_cmds.data();
    if (wait_semaphore) {
        VkSemaphore vk_wait_semaphore = static_cast<RHIVKSemaphore*>(wait_semaphore)->get_vk();
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = &vk_wait_semaphore;
    }
    VkFence vk_fence = fence ? static_cast<RHIVKFence*>(fence)->get_vk() : VK_NULL_HANDLE;
    vkQueueSubmit(m_queue, 1, &submit_info, vk_fence);
}

void RHIVKQueue::wait_idle() {
    vkQueueWaitIdle(m_queue);
}


void RHIVKQueue::submit_and_wait(rhi::RHICommandBuffer* cmd) {
    auto* vk_cmd = static_cast<RHIVKCommandBuffer*>(cmd);
    VkCommandBuffer vk_cmd_buf = vk_cmd->get_vk();
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &vk_cmd_buf;
    vkQueueSubmit(m_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);
}

} // namespace rhi::vulkan
