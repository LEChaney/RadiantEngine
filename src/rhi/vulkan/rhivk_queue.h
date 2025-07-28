
#pragma once
#include "rhi/rhi_queue.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKQueue : public RHIQueue {
public:
    static UniquePtr<RHIVKQueue> createUnique(RHIVKContext* context, uint32 queueFamilyIndex);
    ~RHIVKQueue() override;

    void submit(const Array<RHICommandBuffer*>& commandBuffers, RHIFence* fence, RHISemaphore* waitSemaphore) override;
    void waitIdle() override;
    void submitAndWait(RHICommandBuffer* cmd) override;

    VkQueue getVk() const { return m_queue; }
    uint32 getVkQueueFamilyIndex() const { return m_queueFamilyIndex; }
    
protected:
    RHIVKQueue(RHIVKContext* context, uint32 queueFamilyIndex);
    RHIVKQueue(const RHIVKQueue&) = delete;
    RHIVKQueue& operator=(const RHIVKQueue&) = delete;
    RHIVKQueue(RHIVKQueue&&) = delete;
    RHIVKQueue& operator=(RHIVKQueue&&) = delete;

private:
    VkQueue m_queue;
    uint32 m_queueFamilyIndex;
    RHIVKContext* m_context;
};

} // namespace rhi::vulkan
