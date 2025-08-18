#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"

struct SDL_Window;

namespace RHI {
class RHIImage;
class RHIImageView;
class RHICommandBuffer;
class RHIFence;
class RHISemaphore;
class RHIContext;

struct RHISwapchainCreateInfo {
    SDL_Window* window = nullptr;
    uint32 width = 640;
    uint32 height = 480;
    uint32 imageCount = 3;
    RHIFormat depthFormat = RHIFormat::RHI_FORMAT_D32_SFLOAT;
    RHIImageUsageFlags extraColorUsage = 0;
    RHIImageUsageFlags extraDepthUsage = 0;
};

class RHISwapchain {
public:
    virtual ~RHISwapchain() = default;

    RHISwapchain(const RHISwapchain&) = delete;
    RHISwapchain& operator=(const RHISwapchain&) = delete;
    RHISwapchain(RHISwapchain&&) = delete;
    RHISwapchain& operator=(RHISwapchain&&) = delete;

    virtual uint32 acquireNextImage(RHISemaphore* imageAvailableSemaphore) = 0;
    virtual void present(uint32 imageIndex, RHISemaphore* waitSemaphore) = 0;
    virtual RHIImage* getColorImage(uint32 imageIndex) = 0;
    virtual RHIImageView* getColorImageView(uint32 imageIndex) = 0;
    virtual RHIImage* getDepthImage(uint32 imageIndex) = 0;
    virtual RHIImageView* getDepthImageView(uint32 imageIndex) = 0;
    virtual uint32 imageCount() const = 0;
    virtual void resize(uint32 width, uint32 height) = 0;

    virtual RHIFormat getColorFormat() const = 0;
    virtual RHIFormat getDepthFormat() const = 0;
    virtual RHIColorSpace getColorSpace() const = 0;
    virtual RHISurfaceFormat getSurfaceFormat() const = 0;

protected:
    // Only derived context or implementation should create RHISwapchain objects
    RHISwapchain() = default;
};
} // namespace rhi
