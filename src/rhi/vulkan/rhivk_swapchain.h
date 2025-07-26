#pragma once
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_swapchain.h"
#include "rhi/vulkan/rhivk_image_view.h"
#include "rhi/vulkan/rhivk_image.h"
#include "rhi/rhi_swapchain.h"

#include "core/core_defs.h"

class SDL_Window;

namespace rhi {
namespace vulkan {
class RHIVKSwapchain : public RHISwapchain {
public:
    RHIVKSwapchain(RHIVKContext* context, SDL_Window* window, uint32_t width, uint32_t height, uint32_t image_count);
    ~RHIVKSwapchain() override;

    RHIVKSwapchain(const RHIVKSwapchain&) = delete;
    RHIVKSwapchain& operator=(const RHIVKSwapchain&) = delete;
    RHIVKSwapchain(RHIVKSwapchain&&) = delete;
    RHIVKSwapchain& operator=(RHIVKSwapchain&&) = delete;

    RHIFrame acquire_next_frame() override;
    void present(const RHIFrame& frame) override;
    uint32_t image_count() const override;
    void resize(uint32_t width, uint32_t height) override;

private:
    RHIVKContext* m_rhi_context;
    Array<UniquePtr<RHIVKCommandBuffer>> m_rhi_command_buffers;
    Array<UniquePtr<RHIVKImage>> m_rhi_images;
    Array<UniquePtr<RHIVKImageView>> m_rhi_image_views;
    VkSurfaceKHR m_surface;
    VkSwapchainKHR m_swapchain;
    uint32_t m_image_count;
    uint32_t m_frame_index;
};
} // namespace vulkan
} // namespace rhi
