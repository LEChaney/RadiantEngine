
#pragma once
#include "rhi/rhi_queue.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKQueue : public RHIQueue {
public:
    static UniquePtr<RHIVKQueue> create_unique(RHIVKContext* context, uint32 queue_family_index);
    ~RHIVKQueue() override;

    void submit(const Array<RHICommandBuffer*>& command_buffers, RHIFence* fence, RHISemaphore* wait_semaphore) override;
    void wait_idle() override;
    void submit_and_wait(RHICommandBuffer* cmd) override;

    VkQueue get_vk() const { return m_queue; }
    uint32 get_vk_queue_family_index() const { return m_queue_family_index; }
    
protected:
    RHIVKQueue(RHIVKContext* context, uint32 queue_family_index);
    RHIVKQueue(const RHIVKQueue&) = delete;
    RHIVKQueue& operator=(const RHIVKQueue&) = delete;
    RHIVKQueue(RHIVKQueue&&) = delete;
    RHIVKQueue& operator=(RHIVKQueue&&) = delete;

private:
    VkQueue m_queue;
    uint32 m_queue_family_index;
    RHIVKContext* m_context;
};

} // namespace rhi::vulkan
