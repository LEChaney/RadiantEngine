#pragma once
#include "core/CoreDefs.h"

namespace rhi {
class RHIImage;
class RHIImageView;
class RHICommandBuffer;
class RHIFence;

class RHISwapchain {
public:
    virtual uint32 acquireNextImage(RHISemaphore* imageAvailableSemaphore) = 0;
    virtual void present(uint32 imageIndex, RHISemaphore* waitSemaphore) = 0;
    virtual RHIImage* getImage(uint32 imageIndex) = 0;
    virtual RHIImageView* getImageView(uint32 imageIndex) = 0;
    virtual uint32 imageCount() const = 0;
    virtual void resize(uint32 width, uint32 height) = 0;

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
