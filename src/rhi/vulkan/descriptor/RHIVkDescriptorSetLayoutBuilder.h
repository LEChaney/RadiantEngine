
#pragma once
#include "rhi/interface/descriptor/RHIDescriptorSetLayoutBuilder.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include "core/CoreDefs.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi::vulkan {

class RHIVkContext;

class RHIVkDescriptorSetLayoutBuilder : public rhi::RHIDescriptorSetLayoutBuilder {
public:
    static UniquePtr<RHIVkDescriptorSetLayoutBuilder> createUnique(RHIVkContext* context);

    ~RHIVkDescriptorSetLayoutBuilder() override;

    RHIVkDescriptorSetLayoutBuilder(const RHIVkDescriptorSetLayoutBuilder&) = delete;
    RHIVkDescriptorSetLayoutBuilder& operator=(const RHIVkDescriptorSetLayoutBuilder&) = delete;
    RHIVkDescriptorSetLayoutBuilder(RHIVkDescriptorSetLayoutBuilder&&) = delete;
    RHIVkDescriptorSetLayoutBuilder& operator=(RHIVkDescriptorSetLayoutBuilder&&) = delete;

    RHIVkDescriptorSetLayoutBuilder& addBinding(uint32 binding, RHIDescriptorType type, RHIShaderStageFlags stageFlags = 0) override;
    UniquePtr<RHIDescriptorSetLayout> build(RHIShaderStageFlags layoutStageFlags = 0) override;

private:
    RHIVkDescriptorSetLayoutBuilder(RHIVkContext* context);

    RHIVkContext* m_context = nullptr;
    Array<VkDescriptorSetLayoutBinding> m_bindings;
};

} // namespace rhi::vulkan
