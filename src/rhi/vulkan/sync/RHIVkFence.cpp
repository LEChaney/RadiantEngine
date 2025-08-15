#include "rhi/vulkan/sync/RHIVkFence.h"

namespace RHI::Vulkan {

UniquePtr<RHIVkFence> RHIVkFence::createUnique(RHIVkContext* context) {
    return UniquePtr<RHIVkFence>(new RHIVkFence(context));
}

RHIVkFence::RHIVkFence(RHIVkContext* context)
    : m_context(context)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start in signaled state

    if (vkCreateFence(context->getVkDevice(), &fenceInfo, nullptr, &m_fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan fence");
    }
}

RHIVkFence::~RHIVkFence()
{
    if (m_fence) {
        vkDestroyFence(m_context->getVkDevice(), m_fence, nullptr);
    }
}

void RHIVkFence::wait()
{
    vkWaitForFences(m_context->getVkDevice(), 1, &m_fence, VK_TRUE, UINT64_MAX);
}

void RHIVkFence::reset()
{
    vkResetFences(m_context->getVkDevice(), 1, &m_fence);
}

bool RHIVkFence::isSignaled() const
{
    VkResult result = vkGetFenceStatus(m_context->getVkDevice(), m_fence);
    return result == VK_SUCCESS;
}

} // namespace rhi::vulkan