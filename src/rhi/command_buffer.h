#pragma once

namespace rhi {

class CommandBuffer {
public:
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual ~CommandBuffer() = default;
};

} // namespace rhi
