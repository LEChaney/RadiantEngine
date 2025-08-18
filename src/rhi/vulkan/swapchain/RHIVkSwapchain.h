#pragma once
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/command/RHIVkCommandBuffer.h"
#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/interface/swapchain/RHISwapchain.h"
#include "core/CoreDefs.h"

class SDL_Window;

namespace RHI {
struct RHISwapchainCreateInfo;
}

namespace RHI::Vulkan {
class RHIVkSwapchain : public RHISwapchain {
public:

    static UniquePtr<RHIVkSwapchain> createUnique(
        RHIVkContext* ctx,
        const RHISwapchainCreateInfo& info);
    ~RHIVkSwapchain() override;

    RHIVkSwapchain(const RHIVkSwapchain&) = delete;
    RHIVkSwapchain& operator=(const RHIVkSwapchain&) = delete;
    RHIVkSwapchain(RHIVkSwapchain&&) = delete;
    RHIVkSwapchain& operator=(RHIVkSwapchain&&) = delete;

    uint32 acquireNextImage(RHISemaphore* imageAvailableSemaphore) override;
    void present(uint32 imageIndex, RHISemaphore* waitSemaphore) override;
    uint32_t imageCount() const override;
    void resize(uint32_t width, uint32_t height) override;

    RHIFormat getColorFormat() const override;
    RHIFormat getDepthFormat() const override;
    RHIColorSpace getColorSpace() const override;
    RHISurfaceFormat getSurfaceFormat() const override;

    RHIImage* getColorImage(uint32 imageIndex) override;
    RHIImageView* getColorImageView(uint32 imageIndex) override;
    RHIImage* getDepthImage(uint32 imageIndex) override;
    RHIImageView* getDepthImageView(uint32 imageIndex) override;

private:
    RHIVkSwapchain(
        RHIVkContext* context,
        const RHISwapchainCreateInfo& info);

    RHIVkContext* m_ctx = nullptr;
    SmallArray<UniquePtr<RHIVkImage>, 4> m_colorImgs;
    SmallArray<UniquePtr<RHIVkImageView>, 4> m_colorImgViews;
    SmallArray<UniquePtr<RHIVkImage>, 4> m_depthImgs;
    SmallArray<UniquePtr<RHIVkImageView>, 4> m_depthImgViews;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_surfaceFormat = {};
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    uint32_t m_imageCount = 0;
    uint32_t m_imageIndex = 0;
};
} // namespace rhi::vulkan
