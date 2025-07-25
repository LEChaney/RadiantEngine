
#pragma once
#include "rhi/rhi_semaphore.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKSemaphore : public rhi::RHISemaphore {
public:
    RHIVKSemaphore(VkSemaphore semaphore, VkDevice device);
    ~RHIVKSemaphore();
    VkSemaphore get_vk() const { return m_semaphore; }
private:
    VkSemaphore m_semaphore;
    VkDevice m_device;
};

} // namespace rhi::vulkan
