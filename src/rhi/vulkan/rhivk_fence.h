
#pragma once
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/rhi_fence.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKFence : public rhi::RHIFence {
public:
    static UniquePtr<RHIVKFence> create_unique(RHIVKContext* context);
    ~RHIVKFence();

    const VkFence& get_vk() const { return m_fence; }

    void wait() override;
    void reset() override;
    bool is_signaled() const override;

protected:
    RHIVKFence(RHIVKContext* context);
    RHIVKFence(const RHIVKFence&) = delete;
    RHIVKFence& operator=(const RHIVKFence&) = delete;
    RHIVKFence(RHIVKFence&&) = delete;
    RHIVKFence& operator=(RHIVKFence&&) = delete;

private:
    VkFence m_fence;
    RHIVKContext* m_context;
};

} // namespace rhi::vulkan
