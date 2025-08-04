
#pragma once
#include "rhi/rhi_descriptor_set_layout_builder.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKDescriptorSetLayoutBuilder : public rhi::RHIDescriptorSetLayoutBuilder {
public:
    static UniquePtr<RHIVKDescriptorSetLayoutBuilder> createUnique(RHIVKContext* context);

    ~RHIVKDescriptorSetLayoutBuilder() override;

    RHIVKDescriptorSetLayoutBuilder(const RHIVKDescriptorSetLayoutBuilder&) = delete;
    RHIVKDescriptorSetLayoutBuilder& operator=(const RHIVKDescriptorSetLayoutBuilder&) = delete;
    RHIVKDescriptorSetLayoutBuilder(RHIVKDescriptorSetLayoutBuilder&&) = delete;
    RHIVKDescriptorSetLayoutBuilder& operator=(RHIVKDescriptorSetLayoutBuilder&&) = delete;

    RHIVKDescriptorSetLayoutBuilder& addBinding(uint32 binding, RHIDescriptorType type, RHIShaderStageFlags stageFlags = 0) override;
    UniquePtr<RHIDescriptorSetLayout> build(RHIShaderStageFlags layoutStageFlags = 0) override;

private:
    RHIVKDescriptorSetLayoutBuilder(RHIVKContext* context);

    RHIVKContext* m_context = nullptr;
    Array<VkDescriptorSetLayoutBinding> m_bindings;
};

} // namespace rhi::vulkan
