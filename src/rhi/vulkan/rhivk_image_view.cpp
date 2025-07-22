#include "rhi/vulkan/rhivk_image_view.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

RHIVKImageView::RHIVKImageView(VkImageView imageView, VkDevice device)
    : imageView_(imageView), device_(device) {}

RHIVKImageView::~RHIVKImageView() {
    if (imageView_ && device_) {
        vkDestroyImageView(device_, imageView_, nullptr);
    }
}

} // namespace rhi::vulkan
