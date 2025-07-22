#pragma once
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
    virtual RHIQueue* get_graphics_queue() = 0;
    virtual RHICommandBuffer* create_command_buffer() = 0;
    virtual RHIFence* create_fence() = 0;
    virtual RHISemaphore* create_semaphore() = 0;
    virtual RHISwapchain* create_swapchain(SDL_Window* window, uint32_t width, uint32_t height, uint32_t buffer_count) = 0;
    virtual ~RHIContext() = default;
};

} // namespace rhi
