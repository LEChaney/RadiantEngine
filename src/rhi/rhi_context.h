#pragma once
#include <cstdint>

namespace rhi {

class Queue;
class CommandBuffer;
class Fence;
class Semaphore;
class Swapchain;

class RHIContext {
public:
    virtual Queue* get_graphics_queue() = 0;
    virtual CommandBuffer* create_command_buffer() = 0;
    virtual Fence* create_fence() = 0;
    virtual Semaphore* create_semaphore() = 0;
    virtual Swapchain* create_swapchain(void* window, uint32_t width, uint32_t height, uint32_t buffer_count) = 0;
    virtual ~RHIContext() = default;
};

} // namespace rhi
