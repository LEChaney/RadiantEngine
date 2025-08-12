#pragma once
#include "core/CoreDefs.h"
#include "RHIDescriptorBuffer.h"
#include "RHIDescriptorSet.h"

namespace rhi {

class RHIImageView; // forward (to be added when image view abstraction exists)
class RHISampler;
class RHIBuffer;

class RHIDescriptorWriter {
public:
    virtual ~RHIDescriptorWriter() = default;

    RHIDescriptorWriter(const RHIDescriptorWriter&) = delete;
    RHIDescriptorWriter& operator=(const RHIDescriptorWriter&) = delete;
    RHIDescriptorWriter(RHIDescriptorWriter&&) = delete;
    RHIDescriptorWriter& operator=(RHIDescriptorWriter&&) = delete;

    // Pure virtual interface for writing descriptor records
    virtual RHIDescriptorWriter& writeSampledImage(uint32 binding, uint32 arrayElement, RHIImageView* view, RHISampler* sampler) = 0;
    virtual RHIDescriptorWriter& writeStorageImage(uint32 binding, uint32 arrayElement, RHIImageView* view) = 0;
    virtual RHIDescriptorWriter& writeStorageBuffer(uint32 binding, uint32 arrayElement, RHIBuffer* buffer, size_t offset, size_t range) = 0;
    virtual void flush() = 0; // Flush non-coherent memory if required

protected:
    explicit RHIDescriptorWriter(const RHIDescriptorSet& descSet)
        : m_descSet(descSet) {}

    RHIDescriptorSet m_descSet{};
};

} // namespace rhi
