#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"

namespace rhi {

class RHIDescriptorSetLayout;
class RHIPipelineLayout;

class RHIPipelineLayoutBuilder {
public:
    virtual ~RHIPipelineLayoutBuilder() = default;

    RHIPipelineLayoutBuilder(const RHIPipelineLayoutBuilder&) = delete;
    RHIPipelineLayoutBuilder& operator=(const RHIPipelineLayoutBuilder&) = delete;
    RHIPipelineLayoutBuilder(RHIPipelineLayoutBuilder&&) = delete;
    RHIPipelineLayoutBuilder& operator=(RHIPipelineLayoutBuilder&&) = delete;

    virtual void addDescriptorSetLayout(RHIDescriptorSetLayout* layout) = 0;
    virtual void addPushConstantRange(RHIShaderStageFlags stages, uint32 offset, uint32 size) = 0;
    virtual UniquePtr<RHIPipelineLayout> build() = 0;

protected:
    RHIPipelineLayoutBuilder() = default;
};

} // namespace rhi
