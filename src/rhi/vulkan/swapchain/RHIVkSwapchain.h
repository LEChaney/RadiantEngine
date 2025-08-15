#pragma once
// TODO: Forward declarations
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/command/RHIVkCommandBuffer.h"
#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/sync/RHIVkSemaphore.h"
#include "rhi/vulkan/sync/RHIVkFence.h"
#include "rhi/interface/swapchain/RHISwapchain.h"

#include "core/CoreDefs.h"

class SDL_Window;

namespace RHI::Vulkan {
class RHIVkSwapchain : public RHISwapchain {
public:

    static UniquePtr<RHIVkSwapchain> createUnique(
        RHIVkContext* context, 
        SDL_Window* window, 
        uint32_t width, 
        uint32_t height, 
        uint32_t imageCount,
        RHIImageUsageFlags extraImageUsage = 0);
    ~RHIVkSwapchain() override;

    uint32 acquireNextImage(RHISemaphore* imageAvailableSemaphore) override;
    void present(uint32 imageIndex, RHISemaphore* waitSemaphore) override;
    uint32_t imageCount() const override;
    void resize(uint32_t width, uint32_t height) override;

    RHIFormat getFormat() const override;
    RHIColorSpace getColorSpace() const override;
    RHISurfaceFormat getSurfaceFormat() const override;
    RHIImage* getImage(uint32 imageIndex) override { return m_images[imageIndex].get(); }
    RHIImageView* getImageView(uint32 imageIndex) override { return m_imageViews[imageIndex].get(); }


private:
    RHIVkSwapchain(RHIVkContext* 
        context, 
        SDL_Window* window, 
        uint32_t width, 
        uint32_t height, 
        uint32_t imageCount,
        RHIImageUsageFlags extraImageUsage = 0);
    RHIVkSwapchain(const RHIVkSwapchain&) = delete;
    RHIVkSwapchain& operator=(const RHIVkSwapchain&) = delete;
    RHIVkSwapchain(RHIVkSwapchain&&) = delete;
    RHIVkSwapchain& operator=(RHIVkSwapchain&&) = delete;

    RHIVkContext* m_ctx = nullptr;
    Array<UniquePtr<RHIVkImage>> m_images;
    Array<UniquePtr<RHIVkImageView>> m_imageViews;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_surfaceFormat = {};
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    uint32_t m_imageCount = 0;
    uint32_t m_imageIndex = 0;
};
} // namespace rhi::vulkan
