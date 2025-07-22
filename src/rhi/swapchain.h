#pragma once
#include <cstdint>

namespace rhi {
class ImageView;
class CommandBuffer;

class Swapchain {
public:
    struct Frame {
        uint32_t image_index;
        ImageView* image_view;
        CommandBuffer* command_buffer;
    };

    virtual ~Swapchain() = default;
    virtual Frame acquire_next_frame() = 0;
    virtual void present(const Frame& frame) = 0;
    virtual uint32_t image_count() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
};
} // namespace rhi
