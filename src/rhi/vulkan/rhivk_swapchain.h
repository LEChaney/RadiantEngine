#pragma once
// TODO: Forward declarations
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_image_view.h"
#include "rhi/vulkan/rhivk_image.h"
#include "rhi/vulkan/rhivk_semaphore.h"
#include "rhi/vulkan/rhivk_fence.h"
#include "rhi/rhi_swapchain.h"

#include "core/core_defs.h"

class SDL_Window;

namespace rhi::vulkan {
class RHIVKSwapchain : public RHISwapchain {
public:

    static UniquePtr<RHIVKSwapchain> createUnique(
        RHIVKContext* context, 
        SDL_Window* window, 
        uint32_t width, 
        uint32_t height, 
        uint32_t imageCount);
    ~RHIVKSwapchain() override;

    RHIFrame acquireNextFrame() override;
    void present(const RHIFrame& frame) override;
    uint32_t imageCount() const override;
    void resize(uint32_t width, uint32_t height) override;

    RHIFormat getFormat() const override;
    RHIColorSpace getColorSpace() const override;
    RHISurfaceFormat getSurfaceFormat() const override;


protected:
    RHIVKSwapchain(RHIVKContext* 
        context, 
        SDL_Window* window, 
        uint32_t width, 
        uint32_t height, 
        uint32_t imageCount);
    RHIVKSwapchain(const RHIVKSwapchain&) = delete;
    RHIVKSwapchain& operator=(const RHIVKSwapchain&) = delete;
    RHIVKSwapchain(RHIVKSwapchain&&) = delete;
    RHIVKSwapchain& operator=(RHIVKSwapchain&&) = delete;

private:
    RHIVKContext* m_rhiContext = nullptr;
    Array<UniquePtr<RHIVKCommandBuffer>> m_rhiCommandBuffers;
    Array<UniquePtr<RHIVKImage>> m_rhiImages;
    Array<UniquePtr<RHIVKImageView>> m_rhiImageViews;
    Array<UniquePtr<RHIVKSemaphore>> m_usedRhiAcquireSemaphores;
    Array<UniquePtr<RHIVKSemaphore>> m_freeRhiAcquireSemaphores;
    Array<UniquePtr<RHIVKFence>> m_rhiFences;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR m_surfaceFormat = {};
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    uint32_t m_imageCount = 0;
    uint32_t m_imageIndex = 0;
};
} // namespace rhi::vulkan
