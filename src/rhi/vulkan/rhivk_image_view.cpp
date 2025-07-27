#include "rhi/vulkan/rhivk_image_view.h"
#include "rhi/vulkan/rhivk_image.h"
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

UniquePtr<RHIVKImageView> RHIVKImageView::create_unique(RHIVKContext* context, RHIVKImage* image) {
    return UniquePtr<RHIVKImageView>(new RHIVKImageView(context, image));
}

RHIVKImageView::RHIVKImageView(RHIVKContext* context, RHIVKImage* image) 
    : m_context(context), m_image_view(VK_NULL_HANDLE), m_rhi_image(image) 
{
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = image->get_vk();
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D; // Assuming 2D for simplicity, adjust as needed
    create_info.format = to_vk_format(image->get_format());
    create_info.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                               VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Adjust based on image format
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(context->get_vk_device(), &create_info, nullptr, &m_image_view) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan Image View");
    }
}

RHIVKImageView::~RHIVKImageView() {
    if (m_image_view && m_context) {
        vkDestroyImageView(m_context->get_vk_device(), m_image_view, nullptr);
    }
    // Do not delete m_image here; ownership is external
}

} // namespace rhi::vulkan
