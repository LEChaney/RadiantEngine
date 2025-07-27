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
    
    virtual RHIQueue* get_graphics_queue() = 0;

    // Factory methods for creating RHI objects
    virtual UniquePtr<RHICommandBuffer> create_command_buffer() = 0;
    virtual UniquePtr<RHIFence> create_fence() = 0;
    virtual UniquePtr<RHISemaphore> create_semaphore() = 0;
    virtual UniquePtr<RHISwapchain> create_swapchain(SDL_Window* window, uint32 width, uint32 height, uint32 buffer_count) = 0;
    virtual UniquePtr<RHIBuffer> create_buffer(uint64 size, RHIBufferUsage usage, RHIMemoryProperty mem_props) = 0;
    virtual UniquePtr<RHIImage> create_image(uint32 width, uint32 height, RHIFormat format, RHIImageUsage usage, RHIMemoryProperty mem_props) = 0;
    
protected:
    // Only derived context or implementation should create RHIContext objects
    RHIContext() = default;
    RHIContext(const RHIContext&) = delete;
    RHIContext& operator=(const RHIContext&) = delete;
    RHIContext(RHIContext&&) = delete;
    RHIContext& operator=(RHIContext&&) = delete;
};

} // namespace rhi
