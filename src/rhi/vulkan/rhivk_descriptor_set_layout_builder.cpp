
#include "rhivk_descriptor_set_layout_builder.h"
#include "rhivk_descriptor_set_layout.h"
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include "rhi/rhi_core_defs.h"

namespace rhi::vulkan {

UniquePtr<RHIVKDescriptorSetLayoutBuilder> RHIVKDescriptorSetLayoutBuilder::createUnique(RHIVKContext* context) {
    return UniquePtr<RHIVKDescriptorSetLayoutBuilder>(new RHIVKDescriptorSetLayoutBuilder(context));
}

RHIVKDescriptorSetLayoutBuilder::RHIVKDescriptorSetLayoutBuilder(RHIVKContext* context)
    : m_context(context) {}

RHIVKDescriptorSetLayoutBuilder::~RHIVKDescriptorSetLayoutBuilder() = default;

RHIVKDescriptorSetLayoutBuilder& RHIVKDescriptorSetLayoutBuilder::addBinding(uint32 binding, RHIDescriptorType type, RHIShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding vkBinding{};
    vkBinding.binding = binding;
    vkBinding.descriptorType = toVkDescriptorType(type);
    vkBinding.descriptorCount = 1;
    vkBinding.stageFlags = toVkShaderStageFlags(stageFlags);
    vkBinding.pImmutableSamplers = nullptr;
    m_bindings.push_back(vkBinding);
    return *this;
}

UniquePtr<RHIDescriptorSetLayout> RHIVKDescriptorSetLayoutBuilder::build(RHIShaderStageFlags layoutStageFlags) {
    if (layoutStageFlags != 0) {
        for (auto& binding : m_bindings) {
            binding.stageFlags |= toVkShaderStageFlags(layoutStageFlags);
        }
    }

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = static_cast<uint32_t>(m_bindings.size());
    createInfo.pBindings = m_bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    if (vkCreateDescriptorSetLayout(m_context->getVkDevice(), &createInfo, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    return RHIVKDescriptorSetLayout::createUnique(m_context, layout);
}

} // namespace rhi::vulkan
