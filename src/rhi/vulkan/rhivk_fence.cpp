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

void rhi::vulkan::RHIVKFence::wait()
{
    vkWaitForFences(m_context->get_vk_device(), 1, &m_fence, VK_TRUE, UINT64_MAX);
}

void rhi::vulkan::RHIVKFence::reset()
{
    vkResetFences(m_context->get_vk_device(), 1, &m_fence);
}

bool rhi::vulkan::RHIVKFence::is_signaled() const
{
    VkResult result = vkGetFenceStatus(m_context->get_vk_device(), m_fence);
    return result == VK_SUCCESS;
}
