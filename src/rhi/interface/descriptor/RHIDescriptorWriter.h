#pragma once
#include "rhi/interface/core/RHIContext.h"
#include "core/CoreDefs.h"
#include "RHIDescriptorSetData.h"

namespace RHI {

class RHIImageView;
class RHISampler;
class RHIBuffer;

// Backend-dispatched operations for descriptor writing. Implemented per API (e.g., Vulkan).
struct RHIDescriptorWriterOps {
    RHIDescriptorWriter& (*writeSampledImage)(RHIDescriptorWriter&, uint32 binding, uint32 arrayElement, RHIImageView* view, RHISampler* sampler) = nullptr;
    RHIDescriptorWriter& (*writeStorageImage)(RHIDescriptorWriter&, uint32 binding, uint32 arrayElement, RHIImageView* view) = nullptr;
    RHIDescriptorWriter& (*writeStorageBuffer)(RHIDescriptorWriter&, uint32 binding, uint32 arrayElement, const RHIBufferSlice& buffer) = nullptr;
    void (*flush)(RHIDescriptorWriter&) = nullptr; // Flush non-coherent memory if required
};

class RHIDescriptorWriter {
public:
    // POD-like; trivially copyable/movable. No virtuals, no destructor required.
    RHIDescriptorWriter(RHIContext* ctx, const RHIDescriptorSetData& data)
        : m_ctx(ctx), m_data(data), m_ops(ctx->getDescriptorWriterOps()) {}

    // Shortcut API forwards to backend ops
    RHIDescriptorWriter& writeSampledImage(uint32 binding, uint32 arrayElement, RHIImageView* view, RHISampler* sampler)
    {
        ASSERT(m_ops && m_ops->writeSampledImage && "RHIDescriptorWriter ops not bound");
        return m_ops->writeSampledImage(*this, binding, arrayElement, view, sampler);
    }
    RHIDescriptorWriter& writeStorageImage(uint32 binding, uint32 arrayElement, RHIImageView* view)
    {
        ASSERT(m_ops && m_ops->writeStorageImage && "RHIDescriptorWriter ops not bound");
        return m_ops->writeStorageImage(*this, binding, arrayElement, view);
    }
    RHIDescriptorWriter& writeStorageBuffer(uint32 binding, uint32 arrayElement,
        const RHIBufferSlice& bufferSlice)
    {
        ASSERT(m_ops && m_ops->writeStorageBuffer && "RHIDescriptorWriter ops not bound");
        return m_ops->writeStorageBuffer(*this, binding, arrayElement, bufferSlice);
    }
    void flush() {
        ASSERT(m_ops && m_ops->flush && "RHIDescriptorWriter ops not bound");
        m_ops->flush(*this);
    }

    // Accessors for backend
    RHIContext* getContext() const { return m_ctx; }
    const RHIDescriptorSetData& getData() const { return m_data; }

private:
    RHIContext* m_ctx = nullptr;                   // context kept separately from data
    RHIDescriptorSetData m_data{};                 // value snapshot of the set data (no ctx)
    const RHIDescriptorWriterOps* m_ops = nullptr; // backend function table
};

} // namespace rhi
