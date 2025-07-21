
#pragma once
#include "rhi/semaphore.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKSemaphore : public rhi::Semaphore {
public:
    RHIVKSemaphore(VkSemaphore semaphore, VkDevice device);
    ~RHIVKSemaphore();
    VkSemaphore get_vk() const { return semaphore_; }
private:
    VkSemaphore semaphore_;
    VkDevice device_;
};

} // namespace rhi::vulkan
