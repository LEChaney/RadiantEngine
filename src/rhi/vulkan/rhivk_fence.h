
#pragma once
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/rhi_fence.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKFence : public rhi::RHIFence {
public:
    RHIVKFence(VkFence fence, RHIVKContext* context);
    ~RHIVKFence();

    RHIVKFence(const RHIVKFence&) = delete;
    RHIVKFence& operator=(const RHIVKFence&) = delete;
    RHIVKFence(RHIVKFence&&) = delete;
    RHIVKFence& operator=(RHIVKFence&&) = delete;
    
    const VkFence& get_vk() const { return m_fence; }

    virtual void wait() override;
    virtual void reset() override;
    virtual bool is_signaled() const override;

private:
    VkFence m_fence;
    RHIVKContext* m_context;
};

} // namespace rhi::vulkan
