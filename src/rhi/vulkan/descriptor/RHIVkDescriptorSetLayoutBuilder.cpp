#include "RHIVkDescriptorSetLayoutBuilder.h"
#include "RHIVkDescriptorSetLayout.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include "rhi/interface/core/RHICoreDefs.h"

namespace rhi::vulkan {

UniquePtr<RHIVkDescriptorSetLayoutBuilder> RHIVkDescriptorSetLayoutBuilder::createUnique(RHIVkContext* context) {
    return UniquePtr<RHIVkDescriptorSetLayoutBuilder>(new RHIVkDescriptorSetLayoutBuilder(context));
}

RHIVkDescriptorSetLayoutBuilder::RHIVkDescriptorSetLayoutBuilder(RHIVkContext* context)
    : m_context(context) {}

RHIVkDescriptorSetLayoutBuilder::~RHIVkDescriptorSetLayoutBuilder() = default;

RHIVkDescriptorSetLayoutBuilder& RHIVkDescriptorSetLayoutBuilder::addBinding(uint32 binding, RHIDescriptorType type, RHIShaderStageFlags stageFlags) {
    VkDescriptorSetLayoutBinding vkBinding{};
    vkBinding.binding = binding;
    vkBinding.descriptorType = toVkDescriptorType(type);
    vkBinding.descriptorCount = 1;
    vkBinding.stageFlags = toVkShaderStageFlags(stageFlags);
    vkBinding.pImmutableSamplers = nullptr;
    m_bindings.push_back(vkBinding);
    return *this;
}

UniquePtr<RHIDescriptorSetLayout> RHIVkDescriptorSetLayoutBuilder::build(RHIShaderStageFlags layoutStageFlags) {
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

    return RHIVkDescriptorSetLayout::createUnique(m_context, layout);
}

} // namespace rhi::vulkan
