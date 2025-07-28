#include "rhi/vulkan/rhivk_queue.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_fence.h"
#include "rhi/vulkan/rhivk_semaphore.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace rhi::vulkan {

UniquePtr<RHIVKQueue> RHIVKQueue::createUnique(RHIVKContext* context, uint32 queueFamilyIndex) {
    return UniquePtr<RHIVKQueue>(new RHIVKQueue(context, queueFamilyIndex));
}

RHIVKQueue::RHIVKQueue(RHIVKContext* context, uint32 queueFamilyIndex)
    : m_context(context), m_queueFamilyIndex(queueFamilyIndex)
{
    auto& device = context->getVkDevice();
    vkGetDeviceQueue(device, m_queueFamilyIndex, 0, &m_queue);
}

void RHIVKQueue::submit(const Array<RHICommandBuffer*>& commandBuffers, RHIFence* fence, RHISemaphore* waitSemaphore) {
    Array<VkCommandBuffer> vkCmds;
    for (auto* cmd : commandBuffers) {
        auto* vkCmd = static_cast<RHIVKCommandBuffer*>(cmd);
        vkCmds.push_back(vkCmd->getVk());
    }
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = static_cast<uint32_t>(vkCmds.size());
    submitInfo.pCommandBuffers = vkCmds.data();
    if (waitSemaphore) {
        VkSemaphore vkWaitSemaphore = static_cast<RHIVKSemaphore*>(waitSemaphore)->getVk();
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &vkWaitSemaphore;
    }
    VkFence vkFence = fence ? static_cast<RHIVKFence*>(fence)->getVk() : VK_NULL_HANDLE;
    vkQueueSubmit(m_queue, 1, &submitInfo, vkFence);
}

void RHIVKQueue::waitIdle() {
    vkQueueWaitIdle(m_queue);
}

void RHIVKQueue::submitAndWait(RHICommandBuffer* cmd) {
    auto* vkCmd = static_cast<RHIVKCommandBuffer*>(cmd);
    VkCommandBuffer vkCmdBuf = vkCmd->getVk();
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkCmdBuf;
    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);
}

} // namespace rhi::vulkan
