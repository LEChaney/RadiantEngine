#pragma once
#include "core/CoreDefs.h"
#include "rhi/interface/core/RHICoreDefs.h" // for RHIBufferUsageFlags

namespace rhi {

class RHIBuffer {
public:
    RHIBuffer(uint64 size) : m_size(size) {} // legacy constructor (usage remains 0 until set by derived)
    RHIBuffer(uint64 size, RHIBufferUsageFlags usage) : m_size(size), m_usage(usage) {}
    virtual ~RHIBuffer() = default;
    
    virtual void* map() = 0;
    virtual void unmap() = 0;
    
    uint64 getSize() const { return m_size; }
    RHIBufferUsageFlags getUsage() const { return m_usage; }
    bool hasUsage(RHIBufferUsageFlags usage) const { return (m_usage & usage) == usage; }

protected:
    // Only derived context or implementation should create RHIBuffer objects
    RHIBuffer() = default;
    RHIBuffer(const RHIBuffer&) = delete;
    RHIBuffer& operator=(const RHIBuffer&) = delete;
    RHIBuffer(RHIBuffer&&) = delete;
    RHIBuffer& operator=(RHIBuffer&&) = delete;

    uint64 m_size = 0;
    RHIBufferUsageFlags m_usage = 0;
};

} // namespace rhi
