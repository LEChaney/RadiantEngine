#include "rhivk_fence.h"

namespace rhi::vulkan {

UniquePtr<RHIVKFence> RHIVKFence::createUnique(RHIVKContext* context) {
    return UniquePtr<RHIVKFence>(new RHIVKFence(context));
}

RHIVKFence::RHIVKFence(RHIVKContext* context)
    : m_context(context)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start in signaled state

    if (vkCreateFence(context->getVkDevice(), &fenceInfo, nullptr, &m_fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan fence");
    }
}

RHIVKFence::~RHIVKFence()
{
    if (m_fence) {
        vkDestroyFence(m_context->getVkDevice(), m_fence, nullptr);
    }
}

void RHIVKFence::wait()
{
    vkWaitForFences(m_context->getVkDevice(), 1, &m_fence, VK_TRUE, UINT64_MAX);
}

void RHIVKFence::reset()
{
    vkResetFences(m_context->getVkDevice(), 1, &m_fence);
}

bool RHIVKFence::isSignaled() const
{
    VkResult result = vkGetFenceStatus(m_context->getVkDevice(), m_fence);
    return result == VK_SUCCESS;
}

} // namespace rhi::vulkan