#pragma once
#include "rhi/rhi_image_view.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKImageView : public rhi::RHIImageView {
public:
    RHIVKImageView(VkImageView imageView, VkDevice device);
    ~RHIVKImageView() override;
    VkImageView get_vk() const { return imageView_; }
private:
    VkImageView imageView_;
    VkDevice device_;
};

} // namespace rhi::vulkan
