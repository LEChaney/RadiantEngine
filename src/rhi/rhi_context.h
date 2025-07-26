#pragma once
#include "core/core_defs.h"
#include <cstdint>

class SDL_Window;

namespace rhi {

class RHIQueue;
class RHICommandBuffer;
class RHIFence;
class RHISemaphore;
class RHISwapchain;

class RHIContext {
public:
    RHIContext() = default;
    virtual ~RHIContext() = default;

    RHIContext(const RHIContext&) = delete;
    RHIContext& operator=(const RHIContext&) = delete;
    RHIContext(RHIContext&&) = delete;
    RHIContext& operator=(RHIContext&&) = delete;

    virtual RHIQueue* get_graphics_queue() = 0;

    // Factory methods for creating RHI objects
    virtual UniquePtr<RHICommandBuffer> create_command_buffer() = 0;
    virtual UniquePtr<RHIFence> create_fence() = 0;
    virtual UniquePtr<RHISemaphore> create_semaphore() = 0;
    virtual UniquePtr<RHISwapchain> create_swapchain(SDL_Window* window, uint32_t width, uint32_t height, uint32_t buffer_count) = 0;
};

} // namespace rhi
