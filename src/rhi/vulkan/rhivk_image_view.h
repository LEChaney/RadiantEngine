#pragma once
#include "rhi/rhi_image_view.h"
#include "rhi/vulkan/rhivk_image.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKImage;
class RHIVKContext;

class RHIVKImageView : public rhi::RHIImageView {
public:
    static UniquePtr<RHIVKImageView> createUnique(RHIVKContext* context, RHIVKImage* image);
    ~RHIVKImageView() override;

    RHIVKImageView(const RHIVKImageView&) = delete;
    RHIVKImageView& operator=(const RHIVKImageView&) = delete;
    RHIVKImageView(RHIVKImageView&&) = delete;
    RHIVKImageView& operator=(RHIVKImageView&&) = delete;

    // RHIImageView interface
    rhi::RHIImage* getImage() const override { return m_rhiImage; };
    
    // Vulkan object accessors
    VkImageView getVk() const { return m_imageView; }
    VkImage getVkImage() const { return m_rhiImage->getVk(); }
    
    
private:
    RHIVKImageView(RHIVKContext* context, RHIVKImage* image);
    
    RHIVKContext* m_context;
    VkImageView m_imageView;
    RHIVKImage* m_rhiImage;
};

} // namespace rhi::vulkan
