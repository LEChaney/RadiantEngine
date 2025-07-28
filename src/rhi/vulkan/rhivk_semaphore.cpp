#include "rhivk_semaphore.h"
#include "rhi/vulkan/rhivk_context.h"

namespace rhi::vulkan {

UniquePtr<RHIVKSemaphore> RHIVKSemaphore::createUnique(RHIVKContext* context) {
    return UniquePtr<RHIVKSemaphore>(new RHIVKSemaphore(context));
}

RHIVKSemaphore::RHIVKSemaphore(RHIVKContext* context)
    : m_context(context)
{
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(m_context->getVkDevice(), &createInfo, nullptr, &m_semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan semaphore");
    }
}

RHIVKSemaphore::~RHIVKSemaphore() {
    if (m_semaphore) {
        vkDestroySemaphore(m_context->getVkDevice(), m_semaphore, nullptr);
    }
}

} // namespace rhi::vulkan