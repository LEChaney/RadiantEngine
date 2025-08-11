#pragma once
#include "core/CoreDefs.h"
#include "rhi/interface/pipeline/RHIPipelineLayoutBuilder.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi::vulkan {
    
class RHIVkContext;

class RHIVkPipelineLayoutBuilder : public RHIPipelineLayoutBuilder {
public:
    static UniquePtr<RHIVkPipelineLayoutBuilder> createUnique(RHIVkContext* context);

    ~RHIVkPipelineLayoutBuilder() override = default;

    RHIVkPipelineLayoutBuilder(const RHIVkPipelineLayoutBuilder&) = delete;
    RHIVkPipelineLayoutBuilder& operator=(const RHIVkPipelineLayoutBuilder&) = delete;
    RHIVkPipelineLayoutBuilder(RHIVkPipelineLayoutBuilder&&) = delete;
    RHIVkPipelineLayoutBuilder& operator=(RHIVkPipelineLayoutBuilder&&) = delete;

    void addDescriptorSetLayout(RHIDescriptorSetLayout* layout) override;
    void addPushConstantRange(RHIShaderStageFlags stages, uint32 offset, uint32 size) override;
    UniquePtr<RHIPipelineLayout> build() override;

private:
    RHIVkPipelineLayoutBuilder(RHIVkContext* context);

    RHIVkContext* m_context = nullptr;
    Array<VkDescriptorSetLayout> m_descriptorSetLayouts;
    Array<VkPushConstantRange> m_pushConstantRanges;
};

} // namespace rhi::vulkan
