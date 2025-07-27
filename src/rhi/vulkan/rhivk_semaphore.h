
#pragma once
#include "rhi/rhi_semaphore.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKSemaphore : public RHISemaphore {
public:
    RHIVKSemaphore(VkSemaphore semaphore, RHIVKContext* context);
    ~RHIVKSemaphore();

    RHIVKSemaphore(const RHIVKSemaphore&) = delete;
    RHIVKSemaphore& operator=(const RHIVKSemaphore&) = delete;
    RHIVKSemaphore(RHIVKSemaphore&&) = delete;
    RHIVKSemaphore& operator=(RHIVKSemaphore&&) = delete;
    
    const VkSemaphore& get_vk() const { return m_semaphore; }
    
private:
    VkSemaphore m_semaphore;
    RHIVKContext* m_context;
};

} // namespace rhi::vulkan
