#pragma once
#include "rhi/interface/pipeline/RHIPipelineLayout.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVkContext;

class RHIVkPipelineLayout : public RHIPipelineLayout {
public:
    static UniquePtr<RHIVkPipelineLayout> createUnique(RHIVkContext* context, VkPipelineLayout layout);

    ~RHIVkPipelineLayout() override;

    RHIVkPipelineLayout(const RHIVkPipelineLayout&) = delete;
    RHIVkPipelineLayout& operator=(const RHIVkPipelineLayout&) = delete;
    RHIVkPipelineLayout(RHIVkPipelineLayout&&) = delete;
    RHIVkPipelineLayout& operator=(RHIVkPipelineLayout&&) = delete;

    VkPipelineLayout getVk() const { return m_layout; }

private:
    RHIVkPipelineLayout(RHIVkContext* context, VkPipelineLayout layout);

    RHIVkContext* m_context = nullptr;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

} // namespace rhi::vulkan