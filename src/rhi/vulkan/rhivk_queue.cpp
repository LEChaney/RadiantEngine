#include "rhi/vulkan/rhivk_queue.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_fence.h"
#include "rhi/vulkan/rhivk_semaphore.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace rhi::vulkan {

UniquePtr<RHIVKQueue> RHIVKQueue::create_unique(RHIVKContext* context, uint32 queue_family_index) {
    return UniquePtr<RHIVKQueue>(new RHIVKQueue(context, queue_family_index));
}

RHIVKQueue::RHIVKQueue(RHIVKContext* context, uint32 queue_family_index)
    : m_context(context), m_queue_family_index(queue_family_index)
{
    auto& device = context->get_vk_device();
    vkGetDeviceQueue(device, m_queue_family_index, 0, &m_queue);
}

RHIVKQueue::~RHIVKQueue() {
    // No explicit cleanup needed, Vulkan device destruction handles queue destruction
}

void RHIVKQueue::submit(const Array<RHICommandBuffer*>& command_buffers, RHIFence* fence, RHISemaphore* wait_semaphore) {
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


void RHIVKQueue::submit_and_wait(RHICommandBuffer* cmd) {
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
