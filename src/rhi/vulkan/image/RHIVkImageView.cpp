#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

UniquePtr<RHIVkImageView> RHIVkImageView::createUnique(RHIVkContext* context, RHIVkImage* image) {
    return UniquePtr<RHIVkImageView>(new RHIVkImageView(context, image));
}

RHIVkImageView::RHIVkImageView(RHIVkContext* context, RHIVkImage* image)
    : m_context(context), m_imageView(VK_NULL_HANDLE), m_rhiImage(image)
{
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image->getVk();
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // Assuming 2D for simplicity, adjust as needed
    createInfo.format = toVkFormat(image->getFormat());

    createInfo.components = { .r = VK_COMPONENT_SWIZZLE_R, .g = VK_COMPONENT_SWIZZLE_G,
                              .b = VK_COMPONENT_SWIZZLE_B, .a = VK_COMPONENT_SWIZZLE_A };
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Adjust based on image format
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(context->getVkDevice(), &createInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan Image View");
    }
}

RHIVkImageView::~RHIVkImageView() {
    if (m_imageView && m_context) {
        vkDestroyImageView(m_context->getVkDevice(), m_imageView, nullptr);
    }
    // Do not delete m_rhiImage here; ownership is external
}

} // namespace rhi::vulkan
