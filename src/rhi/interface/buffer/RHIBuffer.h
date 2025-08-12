#pragma once
#include "core/CoreDefs.h"
#include "rhi/interface/core/RHICoreDefs.h"

namespace rhi {

class RHIBuffer {
public:
    RHIBuffer(uint64 size) : m_size(size) {} // legacy constructor (usage remains 0 until set by derived)
    RHIBuffer(uint64 size, RHIBufferUsageFlags usage) : m_size(size), m_usage(usage) {}
    
    virtual ~RHIBuffer() = default;

    RHIBuffer(const RHIBuffer&) = delete;
    RHIBuffer& operator=(const RHIBuffer&) = delete;
    RHIBuffer(RHIBuffer&&) = delete;
    RHIBuffer& operator=(RHIBuffer&&) = delete;
    
    virtual void* map() = 0;
    virtual void unmap() = 0;
    
    bool isMapped() const { return m_mapped != nullptr; }
    void* getMapped() const { return m_mapped; }

    bool isValidAddress(const void* ptr) const {
        return ptr >= getMapped() && ptr < static_cast<uint8*>(getMapped()) + m_size;
    };
    bool isValidRange(const void* ptr, uint64 size) const {
        return ptr >= getMapped()
            && static_cast<const uint8*>(ptr) + size <= static_cast<uint8*>(getMapped()) + m_size;
    };

    uint64 getSize() const { return m_size; }

    RHIBufferUsageFlags getUsage() const { return m_usage; }
    bool hasUsage(RHIBufferUsageFlags usage) const { return (m_usage & usage) == usage; }

protected:
    // Only derived context or implementation should create RHIBuffer objects
    RHIBuffer() = default;

    uint64 m_size = 0;
    RHIBufferUsageFlags m_usage = 0;
    void* m_mapped = nullptr; // pointer to mapped memory, nullptr if not mapped
};

struct RHIBufferSlice {
    RHIBuffer* buffer = nullptr; // pointer to the buffer this slice belongs to
    uint64 offset = 0;
    uint64 size = 0;

    bool isValid() const {
        return buffer && offset + size <= buffer->getSize() && size > 0;
    }

    bool isMapped() const {
        return buffer && buffer->isMapped() && isValid();
    }

    void* getMapped() const {
        return isMapped() ? static_cast<uint8*>(buffer->getMapped()) + offset : nullptr;
    }

    bool isValidAddress(const void* ptr) const {
        return isValid() && ptr >= getMapped() && ptr < static_cast<uint8*>(getMapped()) + size;
    }

    bool isValidRange(const void* ptr, uint64 inSize) const {
        return isValid() && ptr >= getMapped()
            && static_cast<const uint8*>(ptr) + inSize <= static_cast<uint8*>(getMapped()) + size;
    }
};

} // namespace rhi
