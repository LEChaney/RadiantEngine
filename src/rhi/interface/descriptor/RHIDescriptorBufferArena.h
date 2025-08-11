#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"

namespace rhi {

class RHIContext;
class RHIBuffer;
class RHIDescriptorSetLayout;

struct RHIDescriptorSetAllocation {
    class RHIDescriptorBufferArena* arena = nullptr;
    size_t offset = 0;   // byte offset into arena buffer
    size_t size = 0;     // allocation size in bytes
    RHIDescriptorSetLayout* layout = nullptr;
    bool valid() const { return arena && layout && size > 0; }
};

class RHIDescriptorBufferArena {
public:
    struct CreateInfo {
        size_t sizeBytes = 0;
        bool persistentMapped = true;
    };

    virtual ~RHIDescriptorBufferArena() = default;

    RHIDescriptorBufferArena(const RHIDescriptorBufferArena&) = delete;
    RHIDescriptorBufferArena& operator=(const RHIDescriptorBufferArena&) = delete;
    RHIDescriptorBufferArena(RHIDescriptorBufferArena&&) = delete;
    RHIDescriptorBufferArena& operator=(RHIDescriptorBufferArena&&) = delete;

    virtual RHIDescriptorSetAllocation allocateSet(RHIDescriptorSetLayout* layout, const char* debugName = nullptr) = 0;
    virtual void resetLinear() = 0;

    virtual void* mapped() const = 0;
    virtual RHIBuffer* buffer() const = 0;

protected:
    RHIDescriptorBufferArena() = default;
};

} // namespace rhi
