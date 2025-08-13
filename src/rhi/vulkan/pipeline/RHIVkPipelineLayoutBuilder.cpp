#include "RHIVkPipelineLayoutBuilder.h"

#include "rhi/interface/pipeline/RHIPipelineLayoutBuilder.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include "rhi/vulkan/pipeline/RHIVkPipelineLayout.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"

#include <vulkan/vk_enum_string_helper.h>

namespace rhi::vulkan {

namespace {
UniquePtr<RHIPipelineLayout> vkBuildPipelineLayout(
    RHIContext* ctx,
    const Array<RHIDescriptorSetLayout*>& setLayouts,
    const Array<RHIPushConstantRangeDesc>& pushConstants)
{
    auto* vkCtx = static_cast<RHIVkContext*>(ctx);
    VkDevice device = vkCtx->getVkDevice();

    // Convert set layouts
    SmallArray<VkDescriptorSetLayout, 8> vkSetLayouts;
    vkSetLayouts.reserve(setLayouts.size());
    for (auto* h : setLayouts) {
        auto* vkLayout = static_cast<RHIVkDescriptorSetLayout*>(h);
        vkSetLayouts.push_back(vkLayout->getVk());
    }

    // Convert push constant ranges
    SmallArray<VkPushConstantRange, 4> vkPushRanges;
    vkPushRanges.reserve(pushConstants.size());
    for (const auto& pc : pushConstants) {
        VkPushConstantRange range{};
        range.stageFlags = toVkShaderStageFlags(pc.stages);
        range.offset = pc.offset;
        range.size = pc.size;
        vkPushRanges.push_back(range);
    }

    VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    info.setLayoutCount = static_cast<uint32_t>(vkSetLayouts.size());
    info.pSetLayouts = vkSetLayouts.data();
    info.pushConstantRangeCount = static_cast<uint32_t>(vkPushRanges.size());
    info.pPushConstantRanges = vkPushRanges.data();

    VkPipelineLayout vkLayout{};
    VK_CHECK(vkCreatePipelineLayout(device, &info, nullptr, &vkLayout));

    return RHIVkPipelineLayout::createUnique(vkCtx, vkLayout);
}

constexpr RHIPipelineLayoutBuilderOps gk_ops{ &vkBuildPipelineLayout };

} // namespace

const RHIPipelineLayoutBuilderOps* getVkPipelineLayoutBuilderOps() {
    return &gk_ops;
}

} // namespace rhi::vulkan