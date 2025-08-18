#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"

namespace RHI {

class RHIDescriptorSetLayout;

struct RHIPushConstantRangeDesc {
    RHIShaderStageFlags stages = 0;
    uint32 offset = 0;
    uint32 size = 0;
};

struct RHIPipelineLayoutCreateInfo {
    static constexpr uint32 k_inlineSetLayoutsCapacity = 6;
    static constexpr uint32 k_inlinePushConstantRangesCapacity = 1;
    SmallArray<RHIDescriptorSetLayout*, k_inlineSetLayoutsCapacity> setLayouts{};
    SmallArray<RHIPushConstantRangeDesc, k_inlinePushConstantRangesCapacity> pushConstantRanges{};
};

class RHIPipelineLayout {
public:
    virtual ~RHIPipelineLayout() = default;

    RHIPipelineLayout(const RHIPipelineLayout&) = delete;
    RHIPipelineLayout& operator=(const RHIPipelineLayout&) = delete;
    RHIPipelineLayout(RHIPipelineLayout&&) = delete;
    RHIPipelineLayout& operator=(RHIPipelineLayout&&) = delete;

protected:
    RHIPipelineLayout() = default;
};

} // namespace rhi
