#include "rhi/vulkan/sync/RHIVkSemaphore.h"
#include "rhi/vulkan/core/RHIVkContext.h"

namespace rhi::vulkan {

UniquePtr<RHIVkSemaphore> RHIVkSemaphore::createUnique(RHIVkContext* context) {
    return UniquePtr<RHIVkSemaphore>(new RHIVkSemaphore(context));
}

RHIVkSemaphore::RHIVkSemaphore(RHIVkContext* context)
    : m_context(context)
{
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(m_context->getVkDevice(), &createInfo, nullptr, &m_semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan semaphore");
    }
}

RHIVkSemaphore::~RHIVkSemaphore() {
    if (m_semaphore) {
        vkDestroySemaphore(m_context->getVkDevice(), m_semaphore, nullptr);
    }
}

} // namespace rhi::vulkan