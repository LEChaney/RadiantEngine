#pragma once
#include "core/core_defs.h"
#include "rhi/rhi_core_defs.h"

namespace rhi {

class RHIDescriptorSetLayout;

class RHIDescriptorSetLayoutBuilder {
public:
    virtual ~RHIDescriptorSetLayoutBuilder() = default;

    RHIDescriptorSetLayoutBuilder(const RHIDescriptorSetLayoutBuilder&) = delete;
    RHIDescriptorSetLayoutBuilder& operator=(const RHIDescriptorSetLayoutBuilder&) = delete;
    RHIDescriptorSetLayoutBuilder(RHIDescriptorSetLayoutBuilder&&) = delete;
    RHIDescriptorSetLayoutBuilder& operator=(RHIDescriptorSetLayoutBuilder&&) = delete;

    virtual RHIDescriptorSetLayoutBuilder& addBinding(uint32 binding, RHIDescriptorType type, RHIShaderStageFlags stageFlags = 0) = 0;
    virtual UniquePtr<RHIDescriptorSetLayout> build(RHIShaderStageFlags layoutStageFlags = 0) = 0;

protected:
    RHIDescriptorSetLayoutBuilder() = default;
};

} // namespace rhi