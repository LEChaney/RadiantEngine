#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"
#include "rhi/interface/buffer/RHIBuffer.h"
#include <string>

namespace rhi {
class RHIBuffer;
class RHIContext;
class RHIDescriptorSet;
class RHIDescriptorSetLayout;

class RHIDescriptorBuffer {
public:
    struct CreateInfo {
        uint64 sizeBytes = 0;
        bool persistentMapped = true;
        RHIBufferUsageFlags usage = RHIBufferUsage::ResourceDescriptorBuffer;
    };

    virtual ~RHIDescriptorBuffer() = default;

    RHIDescriptorBuffer(const RHIDescriptorBuffer&) = delete;
    RHIDescriptorBuffer& operator=(const RHIDescriptorBuffer&) = delete;
    RHIDescriptorBuffer(RHIDescriptorBuffer&&) = delete;
    RHIDescriptorBuffer& operator=(RHIDescriptorBuffer&&) = delete;

    virtual RHIDescriptorSet allocateSet(RHIDescriptorSetLayout* layout,
        const std::string& debugName = "") = 0;
    virtual void resetLinear() = 0;

    virtual RHIBuffer* getBuffer() const { return m_buffer.get(); };

    void* getMapped() const { return m_buffer->getMapped(); }
    bool isMapped() const { return getMapped() != nullptr; }

    uint64 getSize() const { return getBuffer()->getSize(); }

    bool isValidAddress(const void* ptr) const {
        return ptr >= getMapped()
            && ptr < static_cast<uint8*>(getMapped()) + getBuffer()->getSize();
    };
    bool isValidRange(const void* ptr, size_t size) const {
        return ptr >= getMapped()
            && static_cast<const uint8*>(ptr) + size <= static_cast<uint8*>(getMapped()) + getSize();
    };

    RHIBufferUsageFlags getUsage() const { return getBuffer()->getUsage(); }
    bool hasUsage(RHIBufferUsageFlags usage) const { return getBuffer()->hasUsage(usage); }

protected:
    RHIDescriptorBuffer() = default;

    UniquePtr<RHIBuffer> m_buffer = nullptr;
};

} // namespace rhi
