#include "rhivk_fence.h"

namespace rhi::vulkan {

UniquePtr<RHIVKFence> RHIVKFence::create_unique(RHIVKContext* context) {
    return UniquePtr<RHIVKFence>(new RHIVKFence(context));
}

RHIVKFence::RHIVKFence(RHIVKContext* context)
    : m_context(context)
{
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start in signaled state

    if (vkCreateFence(context->get_vk_device(), &fence_info, nullptr, &m_fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan fence");
    }

}

RHIVKFence::~RHIVKFence()
{
    if (m_fence) {
        vkDestroyFence(m_context->get_vk_device(), m_fence, nullptr);
    }
}

void RHIVKFence::wait()
{
    vkWaitForFences(m_context->get_vk_device(), 1, &m_fence, VK_TRUE, UINT64_MAX);
}

void RHIVKFence::reset()
{
    vkResetFences(m_context->get_vk_device(), 1, &m_fence);
}

bool RHIVKFence::is_signaled() const
{
    VkResult result = vkGetFenceStatus(m_context->get_vk_device(), m_fence);
    return result == VK_SUCCESS;
}

} // namespace rhi::vulkan