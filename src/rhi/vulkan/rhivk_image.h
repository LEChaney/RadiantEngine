#pragma once
#include "rhi/rhi_image.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKImage : public RHIImage {
public:
    static UniquePtr<RHIVKImage> createUnique(
        RHIVKContext* context, 
        uint32 width, 
        uint32 height, 
        RHIFormat format, 
        RHIImageUsageFlags usage, 
        RHIMemoryPropertyFlags memProps
    );
    static UniquePtr<RHIVKImage> createUnique(
        RHIVKContext* context,
        VkImage image,
        uint32 width,
        uint32 height,
        RHIFormat format,
        bool ownsImage = true
    );
        
    ~RHIVKImage() override;

    VkImage getVk() const { return m_image; }
    
protected:
    RHIVKImage(
        RHIVKContext* context,
        uint32 width,
        uint32 height,
        RHIFormat format,
        RHIImageUsageFlags usage,
        RHIMemoryPropertyFlags memProps
    );
    RHIVKImage(
        RHIVKContext* context,
        VkImage image,
        uint32 width,
        uint32 height,
        RHIFormat format,
        bool ownsImage = true
    );
    RHIVKImage(const RHIVKImage&) = delete;
    RHIVKImage& operator=(const RHIVKImage&) = delete;
    RHIVKImage(RHIVKImage&&) = delete;
    RHIVKImage& operator=(RHIVKImage&&) = delete;

private:
    VkImage m_image = VK_NULL_HANDLE;
    RHIVKContext* m_context = nullptr;
    bool m_ownsImage = false; // If true, this class will manage the Vulkan image's lifetime
};

} // namespace rhi::vulkan
