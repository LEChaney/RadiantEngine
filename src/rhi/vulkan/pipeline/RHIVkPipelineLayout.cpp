#include "RHIVkPipelineLayout.h"
#include "rhi/vulkan/core/RHIVkContext.h"

namespace RHI::Vulkan {

UniquePtr<RHIVkPipelineLayout> RHIVkPipelineLayout::createUnique(
    RHIVkContext* context, VkPipelineLayout layout) 
{
    return UniquePtr<RHIVkPipelineLayout>(new RHIVkPipelineLayout(context, layout));
}

RHIVkPipelineLayout::RHIVkPipelineLayout(RHIVkContext* context, VkPipelineLayout layout)
    : m_context(context), m_layout(layout) {}

RHIVkPipelineLayout::~RHIVkPipelineLayout() {
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_context->getVkDevice(), m_layout, nullptr);
    }
}

} // namespace rhi::vulkan