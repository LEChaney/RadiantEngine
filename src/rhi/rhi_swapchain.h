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
        uint32_t imageIndex;
        RHIImage* image;
        RHIImageView* imageView;
        RHICommandBuffer* commandBuffer;
        RHIFence* fence;
    };

    virtual RHIFrame acquireNextFrame() = 0;
    virtual void present(const RHIFrame& frame) = 0;
    virtual uint32_t imageCount() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;

    virtual RHIFormat getFormat() const = 0;
    virtual RHIColorSpace getColorSpace() const = 0;
    virtual RHISurfaceFormat getSurfaceFormat() const = 0;

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
