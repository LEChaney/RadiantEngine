
#pragma once
#include "rhi/rhi_semaphore.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKSemaphore : public RHISemaphore {
public:
    static UniquePtr<RHIVKSemaphore> createUnique(RHIVKContext* context);
    ~RHIVKSemaphore();

    const VkSemaphore& getVk() const { return m_semaphore; }
    
protected:
    RHIVKSemaphore(RHIVKContext* context);
    RHIVKSemaphore(const RHIVKSemaphore&) = delete;
    RHIVKSemaphore& operator=(const RHIVKSemaphore&) = delete;
    RHIVKSemaphore(RHIVKSemaphore&&) = delete;
    RHIVKSemaphore& operator=(RHIVKSemaphore&&) = delete;

private:
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    RHIVKContext* m_context = nullptr;
};

} // namespace rhi::vulkan
