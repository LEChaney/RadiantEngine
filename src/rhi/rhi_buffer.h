#pragma once
#include <cstdint>

namespace rhi {

class RHIBuffer {
public:
    virtual ~RHIBuffer() = default;
    virtual void* map() = 0;
    virtual void unmap() = 0;
};

} // namespace rhi
