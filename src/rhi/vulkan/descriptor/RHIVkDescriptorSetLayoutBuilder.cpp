#include "RHIVkDescriptorSetLayoutBuilder.h"
#include "RHIVkDescriptorSetLayout.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include "rhi/interface/descriptor/RHIDescriptorSetLayoutBuilder.h"

namespace rhi::vulkan {

namespace {

UniquePtr<RHIDescriptorSetLayout> vkBuild(RHIContext* ctxBase, const Array<RHIDescriptorSetBindingDesc>& bindings, RHIShaderStageFlags layoutStageFlags) {
    auto* ctx = static_cast<RHIVkContext*>(ctxBase);

    Array<VkDescriptorSetLayoutBinding> vkBindings;
    vkBindings.reserve(bindings.size());
    for (const auto& b : bindings) {
        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = b.binding;
        vkBinding.descriptorType = toVkDescriptorType(b.type);
        vkBinding.descriptorCount = 1;
        vkBinding.stageFlags = toVkShaderStageFlags(b.stageFlags);
        vkBinding.pImmutableSamplers = nullptr;
        vkBindings.push_back(vkBinding);
    }

    if (layoutStageFlags != 0) {
        for (auto& binding : vkBindings) {
            binding.stageFlags |= toVkShaderStageFlags(layoutStageFlags);
        }
    }

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
    createInfo.pBindings = vkBindings.data();
    createInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT; // for descriptor buffer

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(ctx->getVkDevice(), &createInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    return RHIVkDescriptorSetLayout::createUnique(ctx, layout);
}

constexpr RHIDescriptorSetLayoutBuilderOps gk_ops{ &vkBuild };

} // namespace

const RHIDescriptorSetLayoutBuilderOps* getVkDescriptorSetLayoutBuilderOps() { return &gk_ops; }

} // namespace rhi::vulkan
