#pragma once
#include <cstdint>

namespace rhi {

class RHIBuffer {
public:
    virtual ~RHIBuffer() = default;
    
    virtual void* map() = 0;
    virtual void unmap() = 0;

protected:
    // Only derived context or implementation should create RHIBuffer objects
    RHIBuffer() = default;
    RHIBuffer(const RHIBuffer&) = delete;
    RHIBuffer& operator=(const RHIBuffer&) = delete;
    RHIBuffer(RHIBuffer&&) = delete;
    RHIBuffer& operator=(RHIBuffer&&) = delete;
};

} // namespace rhi
