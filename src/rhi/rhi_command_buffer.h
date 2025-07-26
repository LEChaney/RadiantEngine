#pragma once
#include <glm/vec4.hpp>

namespace rhi {

class RHICommandBuffer {
public:
    RHICommandBuffer() = default;
    virtual ~RHICommandBuffer() = default;

    RHICommandBuffer(const RHICommandBuffer&) = delete;
    RHICommandBuffer& operator=(const RHICommandBuffer&) = delete;
    RHICommandBuffer(RHICommandBuffer&&) = delete;
    RHICommandBuffer& operator=(RHICommandBuffer&&) = delete;

    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void clear_color(class RHIImage* image, const glm::vec4& color) = 0;
};

} // namespace rhi
