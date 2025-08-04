#include "rhivk_context.h"
#include "rhivk_descriptor_set_layout.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

UniquePtr<RHIVKDescriptorSetLayout> RHIVKDescriptorSetLayout::createUnique(RHIVKContext* context, VkDescriptorSetLayout layout) {
    return UniquePtr<RHIVKDescriptorSetLayout>(new RHIVKDescriptorSetLayout(context, layout));
}

RHIVKDescriptorSetLayout::RHIVKDescriptorSetLayout(RHIVKContext* context, VkDescriptorSetLayout layout)
    : m_context(context), m_layout(layout) {}

RHIVKDescriptorSetLayout::~RHIVKDescriptorSetLayout() {
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_context->getVkDevice(), m_layout, nullptr);
    }
}

} // namespace rhi::vulkan
