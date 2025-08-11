#pragma once
#include "rhi/interface/descriptor/RHIDescriptorWriter.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi::vulkan {

class RHIVkDescriptorWriter : public RHIDescriptorWriter {
public:
    static UniquePtr<RHIVkDescriptorWriter> createUnique(const RHIDescriptorSetAllocation& alloc);

    ~RHIVkDescriptorWriter() override = default;

    RHIVkDescriptorWriter(const RHIVkDescriptorWriter&) = delete;
    RHIVkDescriptorWriter& operator=(const RHIVkDescriptorWriter&) = delete;
    RHIVkDescriptorWriter(RHIVkDescriptorWriter&&) = delete;
    RHIVkDescriptorWriter& operator=(RHIVkDescriptorWriter&&) = delete;

    RHIDescriptorWriter& writeSampledImage(uint32 binding, uint32 arrayElement, RHIImageView* view, RHISampler* sampler) override;
    RHIDescriptorWriter& writeStorageImage(uint32 binding, uint32 arrayElement, RHIImageView* view) override;
    RHIDescriptorWriter& writeStorageBuffer(uint32 binding, uint32 arrayElement, RHIBuffer* buffer, size_t offset, size_t range) override;
    void flush() override; // no-op for coherent memory (placeholder)

private:
    RHIVkDescriptorWriter(const RHIDescriptorSetAllocation& alloc);
};

} // namespace rhi::vulkan
