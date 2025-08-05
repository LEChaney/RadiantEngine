#pragma once
#include "core/CoreDefs.h"

namespace rhi {

class RHIBuffer {
public:
    RHIBuffer(uint64 size) : m_size(size) {}
    virtual ~RHIBuffer() = default;
    
    virtual void* map() = 0;
    virtual void unmap() = 0;
    
    uint64 getSize() const { return m_size; }

protected:
    // Only derived context or implementation should create RHIBuffer objects
    RHIBuffer() = default;
    RHIBuffer(const RHIBuffer&) = delete;
    RHIBuffer& operator=(const RHIBuffer&) = delete;
    RHIBuffer(RHIBuffer&&) = delete;
    RHIBuffer& operator=(RHIBuffer&&) = delete;

    uint64 m_size = 0;
};

} // namespace rhi
