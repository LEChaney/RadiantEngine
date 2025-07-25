
#pragma once
#include "rhi/rhi_queue.h"
#include <vector>
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKQueue : public rhi::RHIQueue {
public:
    RHIVKQueue(VkQueue queue, VkDevice device);
    void submit(const std::vector<rhi::RHICommandBuffer*>& command_buffers, rhi::RHIFence* fence, rhi::RHISemaphore* wait_semaphore) override;
    void wait_idle() override;
private:
    VkQueue m_queue;
    VkDevice m_device;
};

} // namespace rhi::vulkan
