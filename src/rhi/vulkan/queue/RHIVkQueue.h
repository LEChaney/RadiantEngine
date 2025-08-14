
#pragma once
#include "rhi/interface/queue/RHIQueue.h"
#include "core/CoreDefs.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi::vulkan {

class RHIVkContext;

class RHIVkQueue : public RHIQueue {
public:
    static UniquePtr<RHIVkQueue> createUnique(RHIVkContext* context, uint32 queueFamilyIndex);
    
    ~RHIVkQueue() override = default;

    RHIVkQueue(const RHIVkQueue&) = delete;
    RHIVkQueue& operator=(const RHIVkQueue&) = delete;
    RHIVkQueue(RHIVkQueue&&) = delete;
    RHIVkQueue& operator=(RHIVkQueue&&) = delete;

    void submit(const SmallArray<RHICommandBuffer*, 1>& commandBuffers, RHISemaphore* waitSemaphore,
        RHISemaphore* signalSemaphore, RHIFence* fence) override;
    void submit(const RHIQueueSubmitDesc& desc) override; // extended submit
    void waitIdle() override;
    void submitAndWait(RHICommandBuffer* cmd) override;

    VkQueue getVk() const { return m_queue; }
    uint32 getVkQueueFamilyIndex() const { return m_queueFamilyIndex; }
    
private:
    RHIVkQueue(RHIVkContext* context, uint32 queueFamilyIndex);

    VkQueue m_queue = VK_NULL_HANDLE;
    uint32 m_queueFamilyIndex = 0;
    RHIVkContext* m_context = nullptr;
};

} // namespace rhi::vulkan
