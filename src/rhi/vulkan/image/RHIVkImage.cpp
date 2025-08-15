#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace RHI::Vulkan {

UniquePtr<RHIVkImage> RHIVkImage::createUnique(
    RHIVkContext* context,
    uint32 width,
    uint32 height,
    RHIFormat format,
    RHIImageUsageFlags usage,
    RHIMemoryPropertyFlags memProps)
{
    return UniquePtr<RHIVkImage>(new RHIVkImage(context, width, height, format, usage, memProps));
}

UniquePtr<RHIVkImage> RHIVkImage::createUnique(
    RHIVkContext* context,
    VkImage image,
    uint32 width,
    uint32 height,
    RHIFormat format,
    RHIImageUsageFlags usage,
    bool ownsImage)
{
    return UniquePtr<RHIVkImage>(new RHIVkImage(context, image, width, height, format, usage, ownsImage));
}

RHIVkImage::RHIVkImage(
    RHIVkContext* context,
    uint32 width,
    uint32 height,
    RHIFormat format,
    RHIImageUsageFlags usage,
    RHIMemoryPropertyFlags memProps)
    : RHIImage(width, height, format, usage)
    , m_context(context), m_ownsImage(true)
{
    // Vulkan image creation logic goes here
    // For example, create VkImage and allocate memory
    // TODO: Implement the actual Vulkan image creation logic
}

RHIVkImage::RHIVkImage(
    RHIVkContext* context,
    VkImage image,
    uint32 width,
    uint32 height,
    RHIFormat format,
    RHIImageUsageFlags usage,
    bool ownsImage)
    : RHIImage(width, height, format, usage)
    , m_image(image), m_context(context), m_ownsImage(ownsImage)
{}

RHIVkImage::~RHIVkImage() {
    if (m_ownsImage && m_image && m_context) {
        vkDestroyImage(m_context->getVkDevice(), m_image, nullptr);
    }
}

} // namespace rhi::vulkan
