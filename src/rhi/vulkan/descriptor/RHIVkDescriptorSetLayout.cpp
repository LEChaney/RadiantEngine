#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"

namespace RHI::Vulkan {

UniquePtr<RHIVkDescriptorSetLayout> RHIVkDescriptorSetLayout::createUnique(RHIVkContext* ctx,
    InitializerList<RHIDescriptorSetBindingDesc> bindings)
{
    Array<VkDescriptorSetLayoutBinding> vkBindings;
    vkBindings.reserve(bindings.size());
    for (const auto& b : bindings) {
        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = b.binding;
        vkBinding.descriptorType = toVkDescriptorType(b.type);
        vkBinding.descriptorCount = 1;
        vkBinding.stageFlags = toVkShaderStageFlags(b.stages);
        vkBinding.pImmutableSamplers = nullptr;
        vkBindings.push_back(vkBinding);
    }
    VkDescriptorSetLayoutCreateInfo ci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    ci.bindingCount = static_cast<uint32_t>(vkBindings.size());
    ci.pBindings = vkBindings.data();
    ci.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(ctx->getVkDevice(), &ci, nullptr, &layout));
    return UniquePtr<RHIVkDescriptorSetLayout>(new RHIVkDescriptorSetLayout(ctx, layout));
}

RHIVkDescriptorSetLayout::RHIVkDescriptorSetLayout(RHIVkContext* ctx, VkDescriptorSetLayout layout)
    : m_ctx(ctx), m_layout(layout) {}

RHIVkDescriptorSetLayout::~RHIVkDescriptorSetLayout() {
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_ctx->getVkDevice(), m_layout, nullptr);
    }
}

} // namespace rhi::vulkan
