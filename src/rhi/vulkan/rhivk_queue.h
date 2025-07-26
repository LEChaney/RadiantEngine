
#pragma once
#include "rhi/rhi_queue.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKQueue : public rhi::RHIQueue {
public:
    RHIVKQueue(VkQueue queue, RHIVKContext* context);
    ~RHIVKQueue() override;
    
    RHIVKQueue(const RHIVKQueue&) = delete;
    RHIVKQueue& operator=(const RHIVKQueue&) = delete;
    RHIVKQueue(RHIVKQueue&&) = delete;
    RHIVKQueue& operator=(RHIVKQueue&&) = delete;

    void submit(const Array<rhi::RHICommandBuffer*>& command_buffers, rhi::RHIFence* fence, rhi::RHISemaphore* wait_semaphore) override;
    void wait_idle() override;
private:
    VkQueue m_queue;
    RHIVKContext* m_context;
};

} // namespace rhi::vulkan
