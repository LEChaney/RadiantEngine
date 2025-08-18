#pragma once

namespace RHI {

struct RHIDescriptorSetBindingDesc {
    uint32 binding = 0;
    RHIDescriptorType type = RHIDescriptorType::Sampler;
    RHIShaderStageFlags stages = 0;
};

class RHIDescriptorSetLayout {
public:
    virtual ~RHIDescriptorSetLayout() = default;

    RHIDescriptorSetLayout(const RHIDescriptorSetLayout&) = delete;
    RHIDescriptorSetLayout& operator=(const RHIDescriptorSetLayout&) = delete;
    RHIDescriptorSetLayout(RHIDescriptorSetLayout&&) = delete;
    RHIDescriptorSetLayout& operator=(RHIDescriptorSetLayout&&) = delete;

protected:
    RHIDescriptorSetLayout() = default;
};

} // namespace rhi