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
    , m_ctx(context), m_ownsImage(true)
{
    ASSERT(m_ctx && "RHIVkImage requires valid context");
    VkFormat vkFormat = toVkFormat(format);
    VkImageUsageFlags vkUsage = toVkImageUsageFlags(usage);

    VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = vkFormat;
    imgInfo.extent = { width, height, 1u };
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = vkUsage;
    imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = toVkMemoryPropertyFlags(memProps);
    if (memProps.hasFlag(RHIMemoryProperty::HostVisible)) {
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT; // allow mapping if needed
    }

    VmaAllocator allocator = m_ctx->getVmaAllocator();
    VmaAllocationInfo vmaAllocInfo{};
    VkResult res = vmaCreateImage(allocator, &imgInfo, &allocInfo, &m_image, &m_allocation, &vmaAllocInfo);
    VK_CHECK(res);
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
    , m_image(image), m_ctx(context), m_ownsImage(ownsImage)
{}

RHIVkImage::~RHIVkImage() {
    if (m_ownsImage && m_image && m_ctx && m_allocation) {
        vmaDestroyImage(m_ctx->getVmaAllocator(), m_image, m_allocation);
    }
}

} // namespace rhi::vulkan
