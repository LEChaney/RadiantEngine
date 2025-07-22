#pragma once
#include <cstdint>

namespace rhi {
class RHIImageView;
class RHICommandBuffer;

class RHISwapchain {
public:
    struct RHIFrame {
        uint32_t image_index;
        RHIImageView* image_view;
        RHICommandBuffer* command_buffer;
    };

    virtual ~RHISwapchain() = default;
    virtual RHIFrame acquire_next_frame() = 0;
    virtual void present(const RHIFrame& frame) = 0;
    virtual uint32_t image_count() const = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
};
} // namespace rhi
