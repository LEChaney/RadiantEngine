#include "rhi/vulkan/rhivk_image.h"
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

UniquePtr<RHIVKImage> RHIVKImage::createUnique(
    RHIVKContext* context,
    uint32 width,
    uint32 height,
    RHIFormat format,
    RHIImageUsage usage,
    RHIMemoryProperty memProps)
{
    return UniquePtr<RHIVKImage>(new RHIVKImage(context, width, height, format, usage, memProps));
}

UniquePtr<RHIVKImage> RHIVKImage::createUnique(
    RHIVKContext* context,
    VkImage image,
    uint32 width,
    uint32 height,
    RHIFormat format,
    bool ownsImage)
{
    return UniquePtr<RHIVKImage>(new RHIVKImage(context, image, width, height, format, ownsImage));
}

RHIVKImage::RHIVKImage(
    RHIVKContext* context,
    uint32 width,
    uint32 height,
    RHIFormat format,
    RHIImageUsage usage,
    RHIMemoryProperty memProps)
    : RHIImage(width, height, format)
    , m_context(context), m_ownsImage(true)
{
    // Vulkan image creation logic goes here
    // For example, create VkImage and allocate memory
    // TODO: Implement the actual Vulkan image creation logic
}

RHIVKImage::RHIVKImage(
    RHIVKContext* context,
    VkImage image,
    uint32 width,
    uint32 height,
    RHIFormat format,
    bool ownsImage)
    : RHIImage(width, height, format)
    , m_image(image), m_context(context), m_ownsImage(ownsImage)
{}

RHIVKImage::~RHIVKImage() {
    if (m_ownsImage && m_image && m_context) {
        vkDestroyImage(m_context->getVkDevice(), m_image, nullptr);
    }
}

} // namespace rhi::vulkan
