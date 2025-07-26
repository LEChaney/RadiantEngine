#include "rhivk_fence.h"

rhi::vulkan::RHIVKFence::RHIVKFence(VkFence fence, RHIVKContext* context)
    : m_fence(fence), m_context(context)
{
}

rhi::vulkan::RHIVKFence::~RHIVKFence()
{
    if (m_fence) {
        vkDestroyFence(m_context->get_vk_device(), m_fence, nullptr);
    }
}