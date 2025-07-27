#pragma once
#include "rhi/rhi_image.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKImage : public RHIImage {
public:
    static UniquePtr<RHIVKImage> create_unique(
        RHIVKContext* context, 
        uint32 width, 
        uint32 height, 
        RHIFormat format, 
        RHIImageUsage usage, 
        RHIMemoryProperty mem_props
    );
    static UniquePtr<RHIVKImage> create_unique(
        RHIVKContext* context,
        VkImage image,
        uint32 width,
        uint32 height,
        RHIFormat format,
        bool owns_image = true
    );
        
    ~RHIVKImage() override;

    VkImage get_vk() const { return m_image; }
    
protected:
    RHIVKImage(
        RHIVKContext* context,
        uint32 width,
        uint32 height,
        RHIFormat format,
        RHIImageUsage usage,
        RHIMemoryProperty mem_props
    );
    RHIVKImage(
        RHIVKContext* context,
        VkImage image,
        uint32 width,
        uint32 height,
        RHIFormat format,
        bool owns_image = true
    );
    RHIVKImage(const RHIVKImage&) = delete;
    RHIVKImage& operator=(const RHIVKImage&) = delete;
    RHIVKImage(RHIVKImage&&) = delete;
    RHIVKImage& operator=(RHIVKImage&&) = delete;

private:
    VkImage m_image;
    RHIVKContext* m_context;
    bool m_owns_image; // If true, this class will manage the Vulkan image's lifetime
};

} // namespace rhi::vulkan
