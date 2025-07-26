#include "rhivk_semaphore.h"
#include "rhi/vulkan/rhivk_context.h"

namespace rhi::vulkan {

RHIVKSemaphore::RHIVKSemaphore(VkSemaphore semaphore, RHIVKContext* context) 
    : m_semaphore(semaphore), m_context(context) {
}

RHIVKSemaphore::~RHIVKSemaphore() {
    if (m_semaphore) {
        vkDestroySemaphore(m_context->get_vk_device(), m_semaphore, nullptr);
    }
}

} // namespace rhi::vulkan