#pragma once

namespace rhi {

class RHICommandBuffer {
public:
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual ~RHICommandBuffer() = default;
};

} // namespace rhi
