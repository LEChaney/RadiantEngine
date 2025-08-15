#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace RHI::Vulkan {

UniquePtr<RHIVkDescriptorSetLayout> RHIVkDescriptorSetLayout::createUnique(RHIVkContext* context, VkDescriptorSetLayout layout) {
    return UniquePtr<RHIVkDescriptorSetLayout>(new RHIVkDescriptorSetLayout(context, layout));
}

RHIVkDescriptorSetLayout::RHIVkDescriptorSetLayout(RHIVkContext* context, VkDescriptorSetLayout layout)
    : m_context(context), m_layout(layout) {}

RHIVkDescriptorSetLayout::~RHIVkDescriptorSetLayout() {
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_context->getVkDevice(), m_layout, nullptr);
    }
}

} // namespace rhi::vulkan
