#pragma once
#include <glm/vec4.hpp>
#include "rhi_core_defs.h"

namespace rhi {

class RHICommandBuffer {
public:
    virtual ~RHICommandBuffer() = default;
    
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void reset() = 0;
    virtual void clear_color(class RHIImage* image, const glm::vec4& color) = 0;
    // Transition image layout using internal layout tracking for old layout
    virtual void transition_image_layout(class RHIImage* image, RHIImageLayout new_layout) = 0;
    // Transition image layout with explicit old layout
    virtual void transition_image_layout(class RHIImage* image, RHIImageLayout old_layout, RHIImageLayout new_layout) = 0;
    virtual void copy_image_to_buffer(class RHIImage* image, class RHIBuffer* buffer, uint32_t width, uint32_t height) = 0;

protected:
    // Only derived context or implementation should create RHICommandBuffer objects
    RHICommandBuffer() = default;
    RHICommandBuffer(const RHICommandBuffer&) = delete;
    RHICommandBuffer& operator=(const RHICommandBuffer&) = delete;
    RHICommandBuffer(RHICommandBuffer&&) = delete;
    RHICommandBuffer& operator=(RHICommandBuffer&&) = delete;
};

} // namespace rhi
