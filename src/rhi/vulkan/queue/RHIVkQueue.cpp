#include "rhi/vulkan/queue/RHIVkQueue.h"
#include "rhi/vulkan/command/RHIVkCommandBuffer.h"
#include "rhi/vulkan/sync/RHIVkFence.h"
#include "rhi/vulkan/sync/RHIVkSemaphore.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"
#include "rhi/vulkan/core/RhiVkTypeConversion.h"
#include <vector>

namespace RHI::Vulkan {

UniquePtr<RHIVkQueue> RHIVkQueue::createUnique(RHIVkContext* context, uint32 queueFamilyIndex) {
    return UniquePtr<RHIVkQueue>(new RHIVkQueue(context, queueFamilyIndex));
}

RHIVkQueue::RHIVkQueue(RHIVkContext* context, uint32 queueFamilyIndex)
    : m_context(context), m_queueFamilyIndex(queueFamilyIndex)
{
    auto& device = context->getVkDevice();
    vkGetDeviceQueue(device, m_queueFamilyIndex, 0, &m_queue);
}

void RHIVkQueue::submit(const SmallArray<RHICommandBuffer*, 1>& commandBuffers,
    RHISemaphore* waitSemaphore, RHISemaphore* signalSemaphore, RHIFence* fence) 
{
    if (commandBuffers.empty()) {
        return;
    }

    RHIQueueSubmitDesc desc;
    for (auto* cmd : commandBuffers) {
        desc.commandBuffers.push_back(cmd);
    }
    if (waitSemaphore) {
        RHISemaphoreSubmitInfo waitInfo{};
        waitInfo.semaphore = waitSemaphore;
        waitInfo.stageMask |= RHIPipelineStage::AllCommands; // conservative default
        desc.waits.push_back(waitInfo);
    }
    if (signalSemaphore) {
        RHISemaphoreSubmitInfo signalInfo{};
        signalInfo.semaphore = signalSemaphore;
        signalInfo.stageMask |= RHIPipelineStage::AllCommands;
        desc.signals.push_back(signalInfo);
    }
    desc.fence = fence;
    submit(desc); // delegate to descriptor-based implementation
}

void RHIVkQueue::submit(const RHIQueueSubmitDesc& desc) {
    if (desc.commandBuffers.empty()) {
        return;
    }

    SmallArray<VkCommandBufferSubmitInfo, 1> cmdInfos;
    cmdInfos.reserve(desc.commandBuffers.size());
    for (auto* cmd : desc.commandBuffers) {
        auto* vkCmd = static_cast<RHIVkCommandBuffer*>(cmd);
        VkCommandBufferSubmitInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        info.commandBuffer = vkCmd->getVk();
        cmdInfos.push_back(info);
    }

    SmallArray<VkSemaphoreSubmitInfo, 1> waitInfos;
    waitInfos.reserve(desc.waits.size());
    for (auto& w : desc.waits) {
        if (!w.semaphore) {
            continue;
        }
        VkSemaphoreSubmitInfo waitInfo{};
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitInfo.semaphore = static_cast<RHIVkSemaphore*>(w.semaphore)->getVk();
        waitInfo.stageMask = toVkPipelineStageFlags2(w.stageMask);
        waitInfo.value = w.value; // 0 for binary
        waitInfos.push_back(waitInfo);
    }

    SmallArray<VkSemaphoreSubmitInfo, 1> signalInfos;
    signalInfos.reserve(desc.signals.size());
    for (auto& s : desc.signals) {
        if (!s.semaphore) {
            continue;
        }
        VkSemaphoreSubmitInfo signalInfo{};
        signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalInfo.semaphore = static_cast<RHIVkSemaphore*>(s.semaphore)->getVk();
        signalInfo.stageMask = toVkPipelineStageFlags2(s.stageMask);
        signalInfo.value = s.value;
        signalInfos.push_back(signalInfo);
    }

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = static_cast<uint32_t>(waitInfos.size());
    submitInfo.pWaitSemaphoreInfos = waitInfos.empty() ? nullptr : waitInfos.data();
    submitInfo.commandBufferInfoCount = static_cast<uint32_t>(cmdInfos.size());
    submitInfo.pCommandBufferInfos = cmdInfos.data();
    submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(signalInfos.size());
    submitInfo.pSignalSemaphoreInfos = signalInfos.empty() ? nullptr : signalInfos.data();

    VkFence vkFence = desc.fence ? static_cast<RHIVkFence*>(desc.fence)->getVk() : VK_NULL_HANDLE;
    vkQueueSubmit2(m_queue, 1, &submitInfo, vkFence);
}

void RHIVkQueue::waitIdle() {
    vkQueueWaitIdle(m_queue);
}

void RHIVkQueue::submitAndWait(RHICommandBuffer* cmd) {
    if (!cmd) {
        return;
    }
    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdInfo.commandBuffer = static_cast<RHIVkCommandBuffer*>(cmd)->getVk();

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdInfo;

    vkQueueSubmit2(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);
}

} // namespace rhi::vulkan
