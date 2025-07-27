#include "rhivk_semaphore.h"
#include "rhi/vulkan/rhivk_context.h"

namespace rhi::vulkan {

UniquePtr<RHIVKSemaphore> RHIVKSemaphore::create_unique(RHIVKContext* context) {
    return UniquePtr<RHIVKSemaphore>(new RHIVKSemaphore(context));
}

RHIVKSemaphore::RHIVKSemaphore(RHIVKContext* context) 
    : m_context(context) 
{
    VkSemaphoreCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(m_context->get_vk_device(), &create_info, nullptr, &m_semaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan semaphore");
    }
}

RHIVKSemaphore::~RHIVKSemaphore() {
    if (m_semaphore) {
        vkDestroySemaphore(m_context->get_vk_device(), m_semaphore, nullptr);
    }
}

} // namespace rhi::vulkan