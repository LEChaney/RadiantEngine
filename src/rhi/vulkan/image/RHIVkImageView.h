#pragma once
#include "rhi/interface/image/RHIImageView.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace RHI::Vulkan {

class RHIVkImage;
class RHIVkContext;

class RHIVkImageView : public RHI::RHIImageView {
public:
    static UniquePtr<RHIVkImageView> createUnique(RHIVkContext* context, RHIVkImage* image, VkImageAspectFlags aspect);
    ~RHIVkImageView() override;

    RHIVkImageView(const RHIVkImageView&) = delete;
    RHIVkImageView& operator=(const RHIVkImageView&) = delete;
    RHIVkImageView(RHIVkImageView&&) = delete;
    RHIVkImageView& operator=(RHIVkImageView&&) = delete;

    // RHIImageView interface
    RHI::RHIImage* getImage() const override { return m_rhiImage; };
    
    // Vulkan object accessors
    VkImageView getVk() const { return m_imageView; }
    VkImage getVkImage() const { return m_rhiImage->getVk(); }
    
    
private:
    RHIVkImageView(RHIVkContext* ctx, RHIVkImage* image, VkImageAspectFlags aspect);
    
    RHIVkContext* m_ctx;
    VkImageView m_imageView;
    RHIVkImage* m_rhiImage;
};

} // namespace rhi::vulkan
