#pragma once
#include <cstdint>

namespace rhi {
class RHIImage;
class RHIImageView;
class RHICommandBuffer;
class RHIFence;

class RHISwapchain {
public:
    struct RHIFrame {
        uint32_t image_index;
        RHIImage* image;
        RHIImageView* image_view;
        RHICommandBuffer* command_buffer;
        RHIFence* fence;
    };

    virtual RHIFrame acquire_next_frame() = 0;
    virtual void present(const RHIFrame& frame) = 0;
    virtual uint32_t image_count() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;

    virtual RHIFormat get_format() const = 0;
    virtual RHIColorSpace get_color_space() const = 0;
    virtual RHISurfaceFormat get_surface_format() const = 0;

    virtual ~RHISwapchain() = default;

protected:
    // Only derived context or implementation should create RHISwapchain objects
    RHISwapchain() = default;
    RHISwapchain(const RHISwapchain&) = delete;
    RHISwapchain& operator=(const RHISwapchain&) = delete;
    RHISwapchain(RHISwapchain&&) = delete;
    RHISwapchain& operator=(RHISwapchain&&) = delete;
};
} // namespace rhi
