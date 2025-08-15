#pragma once
#include "rhi/interface/image/RHIImage.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace RHI::Vulkan {

class RHIVkContext;

class RHIVkImage : public RHIImage {
public:
    static UniquePtr<RHIVkImage> createUnique(
        RHIVkContext* context, 
        uint32 width, 
        uint32 height, 
        RHIFormat format, 
        RHIImageUsageFlags usage, 
        RHIMemoryPropertyFlags memProps
    );
    static UniquePtr<RHIVkImage> createUnique(
        RHIVkContext* context,
        VkImage image,
        uint32 width,
        uint32 height,
        RHIFormat format,
        RHIImageUsageFlags usage,
        bool ownsImage = true
    );
        
    ~RHIVkImage() override;

    RHIVkImage(const RHIVkImage&) = delete;
    RHIVkImage& operator=(const RHIVkImage&) = delete;
    RHIVkImage(RHIVkImage&&) = delete;
    RHIVkImage& operator=(RHIVkImage&&) = delete;

    VkImage getVk() const { return m_image; }
    
private:
    RHIVkImage(
        RHIVkContext* context,
        uint32 width,
        uint32 height,
        RHIFormat format,
        RHIImageUsageFlags usage,
        RHIMemoryPropertyFlags memProps
    );
    RHIVkImage(
        RHIVkContext* context,
        VkImage image,
        uint32 width,
        uint32 height,
        RHIFormat format,
        RHIImageUsageFlags usage,
        bool ownsImage
    );

    RHIVkContext* m_context = nullptr;
    VkImage m_image = VK_NULL_HANDLE;
    bool m_ownsImage = false; // If true, this class will manage the Vulkan image's lifetime
};

} // namespace rhi::vulkan
