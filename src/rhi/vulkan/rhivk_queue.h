
#pragma once
#include "rhi/rhi_queue.h"
#include <vector>
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKQueue : public rhi::RHIQueue {
public:
    RHIVKQueue(VkQueue queue, VkDevice device);
    void submit(const std::vector<rhi::RHICommandBuffer*>& commandBuffers, rhi::RHIFence* fence, rhi::RHISemaphore* waitSemaphore) override;
    void wait_idle() override;
private:
    VkQueue queue_;
    VkDevice device_;
};

} // namespace rhi::vulkan
