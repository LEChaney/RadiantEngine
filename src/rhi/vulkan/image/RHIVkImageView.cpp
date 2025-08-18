#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace RHI::Vulkan {

UniquePtr<RHIVkImageView> RHIVkImageView::createUnique(RHIVkContext* context, RHIVkImage* image, VkImageAspectFlags aspect) {
    return UniquePtr<RHIVkImageView>(new RHIVkImageView(context, image, aspect));
}

RHIVkImageView::RHIVkImageView(RHIVkContext* ctx, RHIVkImage* image, VkImageAspectFlags aspect)
    : m_ctx(ctx), m_imageView(VK_NULL_HANDLE), m_rhiImage(image)
{
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image->getVk();
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // Assuming 2D for simplicity, adjust as needed
    createInfo.format = toVkFormat(image->getFormat());

    createInfo.components = { .r = VK_COMPONENT_SWIZZLE_R, .g = VK_COMPONENT_SWIZZLE_G,
                              .b = VK_COMPONENT_SWIZZLE_B, .a = VK_COMPONENT_SWIZZLE_A };
    createInfo.subresourceRange.aspectMask = aspect;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(ctx->getVkDevice(), &createInfo, nullptr, &m_imageView));
}

RHIVkImageView::~RHIVkImageView() {
    if (m_imageView && m_ctx) {
        vkDestroyImageView(m_ctx->getVkDevice(), m_imageView, nullptr);
    }
    // Do not delete m_rhiImage here; ownership is external
}

} // namespace rhi::vulkan
