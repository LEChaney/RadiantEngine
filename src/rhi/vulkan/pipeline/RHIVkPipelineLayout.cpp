#include "RHIVkPipelineLayout.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"

namespace RHI::Vulkan {

UniquePtr<RHIVkPipelineLayout> RHIVkPipelineLayout::createUnique(
    RHIVkContext* ctx, const RHIPipelineLayoutCreateInfo& info)
{
    // Collect VkDescriptorSetLayout handles
    SmallArray<VkDescriptorSetLayout, RHIPipelineLayoutCreateInfo::k_inlineSetLayoutsCapacity> vkSetLayouts;
    vkSetLayouts.reserve(info.setLayouts.size());
    for (auto* l : info.setLayouts) {
        auto* vkLayout = static_cast<RHIVkDescriptorSetLayout*>(l);
        vkSetLayouts.push_back(vkLayout->getVk());
    }

    // Convert push constant ranges
    SmallArray<VkPushConstantRange, RHIPipelineLayoutCreateInfo::k_inlinePushConstantRangesCapacity> vkRanges;
    vkRanges.reserve(info.pushConstantRanges.size());
    for (const auto& pc : info.pushConstantRanges) {
        VkPushConstantRange vkRange{};
        vkRange.stageFlags = toVkShaderStageFlags(pc.stages);
        vkRange.offset = pc.offset;
        vkRange.size = pc.size;
        vkRanges.push_back(vkRange);
    }

    VkPipelineLayoutCreateInfo ci{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    ci.setLayoutCount = static_cast<uint32>(vkSetLayouts.size());
    ci.pSetLayouts = vkSetLayouts.data();
    ci.pushConstantRangeCount = static_cast<uint32>(vkRanges.size());
    ci.pPushConstantRanges = vkRanges.data();
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(ctx->getVkDevice(), &ci, nullptr, &layout));
    return UniquePtr<RHIVkPipelineLayout>(new RHIVkPipelineLayout(ctx, layout));
}

RHIVkPipelineLayout::RHIVkPipelineLayout(RHIVkContext* ctx, VkPipelineLayout layout)
    : m_ctx(ctx), m_layout(layout) {}

RHIVkPipelineLayout::~RHIVkPipelineLayout() {
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_ctx->getVkDevice(), m_layout, nullptr);
    }
}

} // namespace rhi::vulkan