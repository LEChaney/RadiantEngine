#pragma once
#include "rhi/rhi_image_view.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKImageView : public rhi::RHIImageView {
public:
    RHIVKImageView(VkImageView image_view, VkDevice device);
    ~RHIVKImageView() override;
    VkImageView get_vk() const { return m_image_view; }
private:
    VkImageView m_image_view;
    VkDevice m_vk_device;
};

} // namespace rhi::vulkan
