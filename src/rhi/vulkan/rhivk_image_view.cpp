#include "rhi/vulkan/rhivk_image_view.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

RHIVKImageView::RHIVKImageView(VkImageView image_view, VkDevice device)
    : m_image_view(image_view), m_vk_device(device) {}

RHIVKImageView::~RHIVKImageView() {
    if (m_image_view && m_vk_device) {
        vkDestroyImageView(m_vk_device, m_image_view, nullptr);
    }
}

} // namespace rhi::vulkan
