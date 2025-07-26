#pragma once
#include "rhi/rhi_image_view.h"
#include "rhi/vulkan/rhivk_image.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKImage;
class RHIVKContext;

class RHIVKImageView : public rhi::RHIImageView {
public:
    RHIVKImageView(VkImageView image_view, RHIVKImage* image, RHIVKContext* context);
    ~RHIVKImageView() override;

    // RHIImageView interface
    rhi::RHIImage* get_image() const override { return m_rhi_image; };
    
    // Vulkan object accessors
    VkImageView get_vk() const { return m_image_view; }
    VkImage get_vk_image() const { return m_rhi_image->get_vk(); }
    RHIVKImageView(const RHIVKImageView&) = delete;
    RHIVKImageView& operator=(const RHIVKImageView&) = delete;
    RHIVKImageView(RHIVKImageView&&) = delete;
    RHIVKImageView& operator=(RHIVKImageView&&) = delete;
private:
    VkImageView m_image_view;
    RHIVKImage* m_rhi_image;
    RHIVKContext* m_context;
};

} // namespace rhi::vulkan
