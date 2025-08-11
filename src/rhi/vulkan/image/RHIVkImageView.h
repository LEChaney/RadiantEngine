#pragma once
#include "rhi/interface/image/RHIImageView.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi::vulkan {

class RHIVkImage;
class RHIVkContext;

class RHIVkImageView : public rhi::RHIImageView {
public:
    static UniquePtr<RHIVkImageView> createUnique(RHIVkContext* context, RHIVkImage* image);
    ~RHIVkImageView() override;

    RHIVkImageView(const RHIVkImageView&) = delete;
    RHIVkImageView& operator=(const RHIVkImageView&) = delete;
    RHIVkImageView(RHIVkImageView&&) = delete;
    RHIVkImageView& operator=(RHIVkImageView&&) = delete;

    // RHIImageView interface
    rhi::RHIImage* getImage() const override { return m_rhiImage; };
    
    // Vulkan object accessors
    VkImageView getVk() const { return m_imageView; }
    VkImage getVkImage() const { return m_rhiImage->getVk(); }
    
    
private:
    RHIVkImageView(RHIVkContext* context, RHIVkImage* image);
    
    RHIVkContext* m_context;
    VkImageView m_imageView;
    RHIVkImage* m_rhiImage;
};

} // namespace rhi::vulkan
