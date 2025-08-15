#pragma once

namespace RHI {

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