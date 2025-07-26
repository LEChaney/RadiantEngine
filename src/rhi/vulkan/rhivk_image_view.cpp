#include "rhi/vulkan/rhivk_image_view.h"
#include "rhi/vulkan/rhivk_image.h"
#include "rhi/vulkan/rhivk_context.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {


RHIVKImageView::RHIVKImageView(VkImageView image_view, RHIVKImage* image, RHIVKContext* context)
    : m_image_view(image_view), m_rhi_image(image), m_context(context) {}

RHIVKImageView::~RHIVKImageView() {
    if (m_image_view && m_context) {
        vkDestroyImageView(m_context->get_vk_device(), m_image_view, nullptr);
    }
    // Do not delete m_image here; ownership is external
}

} // namespace rhi::vulkan
