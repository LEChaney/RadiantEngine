
#pragma once
#include "rhi/rhi_semaphore.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKSemaphore : public RHISemaphore {
public:
    static UniquePtr<RHIVKSemaphore> create_unique(RHIVKContext* context);
    ~RHIVKSemaphore();

    const VkSemaphore& get_vk() const { return m_semaphore; }
    
protected:
    RHIVKSemaphore(RHIVKContext* context);
    RHIVKSemaphore(const RHIVKSemaphore&) = delete;
    RHIVKSemaphore& operator=(const RHIVKSemaphore&) = delete;
    RHIVKSemaphore(RHIVKSemaphore&&) = delete;
    RHIVKSemaphore& operator=(RHIVKSemaphore&&) = delete;

private:
    VkSemaphore m_semaphore;
    RHIVKContext* m_context;
};

} // namespace rhi::vulkan
