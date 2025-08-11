#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include <glm/vec4.hpp>

namespace rhi {

class RHIPipeline;

class RHICommandBuffer {
public:
    virtual ~RHICommandBuffer() = default;
    
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void reset() = 0;
    virtual void clearColor(class RHIImage* image, const glm::vec4& color) = 0;
    // Transition image layout using internal layout tracking for old layout
    virtual void transitionImageLayout(class RHIImage* image, RHIImageLayout newLayout) = 0;
    // Transition image layout with explicit old layout
    virtual void transitionImageLayout(class RHIImage* image, RHIImageLayout oldLayout, RHIImageLayout newLayout) = 0;
    virtual void copyImageToBuffer(class RHIImage* image, class RHIBuffer* buffer, uint32_t width, uint32_t height) = 0;
    // Bind a compute pipeline
    virtual void bindComputePipeline(RHIPipeline* pipeline) = 0;

protected:
    // Only derived context or implementation should create RHICommandBuffer objects
    RHICommandBuffer() = default;
    RHICommandBuffer(const RHICommandBuffer&) = delete;
    RHICommandBuffer& operator=(const RHICommandBuffer&) = delete;
    RHICommandBuffer(RHICommandBuffer&&) = delete;
    RHICommandBuffer& operator=(RHICommandBuffer&&) = delete;
};

} // namespace rhi
