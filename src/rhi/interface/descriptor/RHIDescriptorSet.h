#pragma once
#include "RHIDescriptorWriter.h"
#include "RHIDescriptorBuffer.h"
#include "RHIDescriptorSetData.h"
#include "core/CoreDefs.h"

namespace RHI {

class RHIDescriptorSetLayout;
class RHISampler;
class RHIImageView; // forward declare image view
class RHIContext; // forward to use getDescriptorWriterOps()

class RHIDescriptorSet {
public:
    RHIDescriptorSet(RHIContext* ctx, RHIDescriptorSetLayout* layout, RHIDescriptorBuffer* buffer,
        uint64 offset, uint64 size)
        : m_ctx(ctx), m_data{ layout, buffer, offset, size }, m_writer(ctx, m_data) {}
    RHIDescriptorSet(RHIContext* ctx, const RHIDescriptorSetData& data)
        : m_ctx(ctx), m_data(data), m_writer(ctx, m_data) {}

    RHIDescriptorSetData getData() const { return m_data; }
    RHIDescriptorSetLayout* getLayout() const { return m_data.layout; }
    RHIDescriptorBuffer* getBuffer() const { return m_data.buffer; }
    uint64 getOffset() const { return m_data.offset; }
    uint64 getSize() const { return m_data.size; }

    RHIDescriptorWriter& writeSampledImage(uint32 binding, uint32 arrayElement, RHIImageView* view,
        RHISampler* sampler) {
        return m_writer.writeSampledImage(binding, arrayElement, view, sampler);
    };
    RHIDescriptorWriter& writeStorageImage(uint32 binding, uint32 arrayElement, RHIImageView* view) {
        return m_writer.writeStorageImage(binding, arrayElement, view);
    };
    RHIDescriptorWriter& writeStorageBuffer(uint32 binding, uint32 arrayElement, RHIBuffer* buffer,
        uint64 offset, uint64 range) {
        return m_writer.writeStorageBuffer(binding, arrayElement, buffer, offset, range);
    };

    bool isMapped() const { return m_data.isMapped(); }
    void* getMapped() const { return m_data.getMapped(); }

    bool isValid() const { return m_data.isValid(); }
    bool isValidAddress(const void* ptr) const { return m_data.isValidAddress(ptr); }
    bool isValidRange(const void* ptr, uint64 inSize) const { return m_data.isValidRange(ptr, inSize); }

private:
    RHIContext* m_ctx = nullptr;     // keep separate context pointer
    RHIDescriptorSetData m_data{};   // value-type descriptor set data (POD)
    RHIDescriptorWriter m_writer;    // value-type writer (POD)
};
struct RHIDescriptorSetBinding {
    uint32 setIndex = 0; // index of the descriptor set this binding belongs to
    RHIDescriptorSet set; // descriptor set allocation
};

} // namespace rhi
