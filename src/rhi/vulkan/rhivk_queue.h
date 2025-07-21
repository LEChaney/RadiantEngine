
#pragma once
#include "rhi/queue.h"
#include <vector>
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKQueue : public rhi::Queue {
public:
    RHIVKQueue(VkQueue queue, VkDevice device);
    void submit(const std::vector<rhi::CommandBuffer*>& commandBuffers, rhi::Fence* fence, rhi::Semaphore* waitSemaphore) override;
    void wait_idle() override;
private:
    VkQueue queue_;
    VkDevice device_;
};

} // namespace rhi::vulkan
