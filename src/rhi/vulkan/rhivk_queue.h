
#pragma once
#include "rhi/rhi_queue.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKQueue : public RHIQueue {
public:
    RHIVKQueue(VkQueue queue, RHIVKContext* context);
    ~RHIVKQueue() override;
    
    RHIVKQueue(const RHIVKQueue&) = delete;
    RHIVKQueue& operator=(const RHIVKQueue&) = delete;
    RHIVKQueue(RHIVKQueue&&) = delete;
    RHIVKQueue& operator=(RHIVKQueue&&) = delete;

    void submit(const Array<RHICommandBuffer*>& command_buffers, RHIFence* fence, RHISemaphore* wait_semaphore) override;
    void wait_idle() override;
    void submit_and_wait(RHICommandBuffer* cmd) override;
private:
    VkQueue m_queue;
    RHIVKContext* m_context;
};

} // namespace rhi::vulkan
