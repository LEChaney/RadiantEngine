#include "RHIVkPipelineLayoutBuilder.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"
#include "rhi/vulkan/pipeline/RHIVkPipelineLayout.h"

namespace rhi::vulkan {

UniquePtr<RHIVkPipelineLayoutBuilder> RHIVkPipelineLayoutBuilder::createUnique(RHIVkContext* context) {
    return UniquePtr<RHIVkPipelineLayoutBuilder>(new RHIVkPipelineLayoutBuilder(context));
}

RHIVkPipelineLayoutBuilder::RHIVkPipelineLayoutBuilder(RHIVkContext* context)
    : m_context(context) {}

void RHIVkPipelineLayoutBuilder::addDescriptorSetLayout(RHIDescriptorSetLayout* layout) 
{
    auto* rhiVkLayout = static_cast<RHIVkDescriptorSetLayout*>(layout);
    m_descriptorSetLayouts.push_back(rhiVkLayout->getVk());
}

void RHIVkPipelineLayoutBuilder::addPushConstantRange(RHIShaderStageFlags stages,
    uint32 offset, uint32 size) 
{
    VkPushConstantRange range = {};
    range.stageFlags = toVkShaderStageFlags(stages);
    range.offset = offset;
    range.size = size;

    m_pushConstantRanges.push_back(range);
}

UniquePtr<RHIPipelineLayout> RHIVkPipelineLayoutBuilder::build() {
    // Build the vulkand pipeline layout here
    VkPipelineLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    // Set the descriptor set layouts
    createInfo.setLayoutCount = m_descriptorSetLayouts.size();
    createInfo.pSetLayouts = m_descriptorSetLayouts.data();

    // Set the push constant ranges
    createInfo.pushConstantRangeCount = static_cast<uint32>(m_pushConstantRanges.size());
    createInfo.pPushConstantRanges = m_pushConstantRanges.data();

    VkPipelineLayout layout;
    if (vkCreatePipelineLayout(m_context->getVkDevice(), &createInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan pipeline layout");
    }

    return RHIVkPipelineLayout::createUnique(m_context, layout);
}

} // namespace rhi::vulkan