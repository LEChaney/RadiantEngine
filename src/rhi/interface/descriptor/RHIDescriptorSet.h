#pragma once
#include "RHIDescriptorBuffer.h"
#include "core/CoreDefs.h"

namespace rhi {

class RHIDescriptorSetLayout;

struct RHIDescriptorSet {
    RHIDescriptorBuffer* buffer = nullptr; // pointer to the buffer this slice belongs to
    uint64 offset = 0;
    uint64 size = 0;
    RHIDescriptorSetLayout* layout = nullptr;

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

struct RHIDescriptorSetBinding {
    uint32 setIndex = 0; // index of the descriptor set this binding belongs to
    RHIDescriptorSet set; // descriptor set allocation
};

} // namespace rhi
