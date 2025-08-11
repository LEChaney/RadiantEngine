#pragma once
#include "rhi/interface/sync/RHISemaphore.h"
#include "core/CoreDefs.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi::vulkan {

class RHIVkContext;

class RHIVkSemaphore : public RHISemaphore {
public:
    static UniquePtr<RHIVkSemaphore> createUnique(RHIVkContext* context);

    ~RHIVkSemaphore();

    RHIVkSemaphore(const RHIVkSemaphore&) = delete;
    RHIVkSemaphore& operator=(const RHIVkSemaphore&) = delete;
    RHIVkSemaphore(RHIVkSemaphore&&) = delete;
    RHIVkSemaphore& operator=(RHIVkSemaphore&&) = delete;

    const VkSemaphore& getVk() const { return m_semaphore; }

private:
    RHIVkSemaphore(RHIVkContext* context);

    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    RHIVkContext* m_context = nullptr;
};

} // namespace rhi::vulkan
