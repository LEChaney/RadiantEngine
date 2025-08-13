#pragma once
#include "core/CoreDefs.h"
#include "rhi/interface/descriptor/RHIDescriptorBuffer.h"

namespace rhi {

class RHIDescriptorSetLayout;

// POD view of a descriptor set allocation
struct RHIDescriptorSetData {
    RHIDescriptorSetLayout* layout = nullptr;
    RHIDescriptorBuffer* buffer = nullptr;
    uint64 offset = 0;
    uint64 size = 0;

    // Helpers (header-only)
    void* getMapped() const {
        if (!buffer->isMapped()) {
            return nullptr;
        }
        return static_cast<uint8*>(buffer->getMapped()) + offset;
    }
    bool isMapped() const {
        return buffer && buffer->isMapped() && isValid();
    }
    bool isValid() const {
        return buffer && (offset + size) <= buffer->getSize() && size > 0;
    }
    bool isValidAddress(const void* ptr) const {
        if (!isMapped()) {
            return false;
        }
        const auto* base = static_cast<const uint8*>(getMapped());
        return ptr >= base && ptr < (base + size);
    }
    bool isValidRange(const void* ptr, uint64 inSize) const {
        if (!isMapped()) {
            return false;
        }
        const auto* base = static_cast<const uint8*>(getMapped());
        const auto* p = static_cast<const uint8*>(ptr);
        return p >= base && (p + inSize) <= (base + size);
    }
};

} // namespace rhi
