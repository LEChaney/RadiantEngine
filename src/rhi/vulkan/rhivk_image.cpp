#include "rhi/vulkan/rhivk_image.h"
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

UniquePtr<RHIVKImage> RHIVKImage::create_unique(
    RHIVKContext* context, 
    uint32 width, 
    uint32 height, 
    RHIFormat format, 
    RHIImageUsage usage, 
    RHIMemoryProperty mem_props) 
{
    return UniquePtr<RHIVKImage>(new RHIVKImage(context, width, height, format, usage, mem_props));
}

UniquePtr<RHIVKImage> RHIVKImage::create_unique(
    RHIVKContext* context,
    VkImage image,
    uint32 width,
    uint32 height,
    RHIFormat format,
    bool owns_image) 
{
    return UniquePtr<RHIVKImage>(new RHIVKImage(context, image, width, height, format, owns_image));
}

RHIVKImage::RHIVKImage(
    RHIVKContext* context,
    uint32 width,
    uint32 height,
    RHIFormat format,
    RHIImageUsage usage,
    RHIMemoryProperty mem_props
)
    : RHIImage(width, height, format)
    , m_context(context), m_owns_image(true)
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
    bool owns_image
)
    : RHIImage(width, height, format)
    , m_image(image), m_context(context), m_owns_image(owns_image)
{}

RHIVKImage::~RHIVKImage() {
    if (m_owns_image && m_image && m_context) {
        vkDestroyImage(m_context->get_vk_device(), m_image, nullptr);
    }
}

} // namespace rhi::vulkan
