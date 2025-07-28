#pragma once
#include "core/core_defs.h"
#include "rhi_core_defs.h"
#include <cstdint>

class SDL_Window;

namespace rhi {

class RHIQueue;
class RHICommandBuffer;
class RHIFence;
class RHISemaphore;
class RHISwapchain;
class RHIBuffer;
class RHIImage;

class RHIContext {
public:
    virtual ~RHIContext() = default;

    virtual RHIQueue* getGraphicsQueue() = 0;

    // Factory methods for creating RHI objects
    virtual UniquePtr<RHICommandBuffer> createCommandBuffer() = 0;
    virtual UniquePtr<RHIFence> createFence() = 0;
    virtual UniquePtr<RHISemaphore> createSemaphore() = 0;
    virtual UniquePtr<RHISwapchain> createSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 bufferCount) = 0;
    virtual UniquePtr<RHIBuffer> createBuffer(uint64 size, RHIBufferUsage usage, RHIMemoryProperty memProps) = 0;
    virtual UniquePtr<RHIImage> createImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsage usage, RHIMemoryProperty memProps) = 0;

protected:
    // Only derived context or implementation should create RHIContext objects
    RHIContext() = default;
    RHIContext(const RHIContext&) = delete;
    RHIContext& operator=(const RHIContext&) = delete;
    RHIContext(RHIContext&&) = delete;
    RHIContext& operator=(RHIContext&&) = delete;
};

} // namespace rhi
