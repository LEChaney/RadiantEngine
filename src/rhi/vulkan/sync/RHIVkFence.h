#pragma once
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/interface/sync/RHIFence.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace RHI::Vulkan {

class RHIVkFence : public RHI::RHIFence {
public:
    static UniquePtr<RHIVkFence> createUnique(RHIVkContext* context);
    ~RHIVkFence();

    RHIVkFence(const RHIVkFence&) = delete;
    RHIVkFence& operator=(const RHIVkFence&) = delete;
    RHIVkFence(RHIVkFence&&) = delete;
    RHIVkFence& operator=(RHIVkFence&&) = delete;

    const VkFence& getVk() const { return m_fence; }

    void wait() override;
    void reset() override;
    bool isSignaled() const override;
    
private:
    RHIVkFence(RHIVkContext* context);
    
    VkFence m_fence = VK_NULL_HANDLE;
    RHIVkContext* m_context = nullptr;
};

} // namespace rhi::vulkan
