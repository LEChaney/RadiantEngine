#include "rhi/vulkan/rhivk_image.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

RHIVKImage::RHIVKImage(VkImage image, VkDevice device, bool owns_image)
    : m_image(image), m_device(device), m_owns_image(owns_image) {}

RHIVKImage::~RHIVKImage() {
    if (m_owns_image && m_image && m_device) {
        vkDestroyImage(m_device, m_image, nullptr);
    }
}

} // namespace rhi::vulkan
