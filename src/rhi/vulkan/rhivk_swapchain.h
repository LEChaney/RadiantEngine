#pragma once
#include "rhi/swapchain.h"

namespace rhi {
namespace vulkan {
class RHIVKSwapchain : public RHISwapchain {
public:
    RHIVKSwapchain(void* window, uint32_t width, uint32_t height, uint32_t buffer_count);
    ~RHIVKSwapchain() override;

    RHIFrame acquire_next_frame() override;
    void present(const RHIFrame& frame) override;
    uint32_t image_count() const override;
    void resize(uint32_t width, uint32_t height) override;

private:
    // Vulkan handles and resources (VkSwapchainKHR, VkImage, VkImageView, etc.)
    // std::vector<VkImage> images;
    // std::vector<VkImageView> image_views;
    // ...
    uint32_t m_image_count;
};
} // namespace vulkan
} // namespace rhi
