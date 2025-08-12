#pragma once
#include "rhi/interface/descriptor/RHIDescriptorWriter.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi { class RHIImageView; class RHISampler; class RHIBuffer; }
namespace rhi::vulkan {
class RHIVkContext;
class RHIVkDescriptorSetLayout;

class RHIVkDescriptorWriter : public RHIDescriptorWriter {
public:
    static UniquePtr<RHIVkDescriptorWriter> createUnique(RHIVkContext* ctx, const RHIDescriptorSet& alloc);
    ~RHIVkDescriptorWriter() override = default;
    RHIVkDescriptorWriter(const RHIVkDescriptorWriter&) = delete; RHIVkDescriptorWriter& operator=(const RHIVkDescriptorWriter&) = delete; RHIVkDescriptorWriter(RHIVkDescriptorWriter&&) = delete; RHIVkDescriptorWriter& operator=(RHIVkDescriptorWriter&&) = delete;
    RHIDescriptorWriter& writeSampledImage(uint32 binding, uint32 arrayElement, RHIImageView* view, RHISampler* sampler) override;
    RHIDescriptorWriter& writeStorageImage(uint32 binding, uint32 arrayElement, RHIImageView* view) override;
    RHIDescriptorWriter& writeStorageBuffer(uint32 binding, uint32 arrayElement, RHIBuffer* buffer, size_t offset, size_t range) override;
    void flush() override {} // coherent for now
private:
    RHIVkDescriptorWriter(RHIVkContext* ctx, const RHIDescriptorSet& alloc);
    
    VkDescriptorSetLayout getVkLayout() const;
    VkDevice getDevice() const;
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer) const;
    size_t bindingOffset(uint32 binding) const; // query via vkGetDescriptorSetLayoutBindingOffsetEXT
    uint8* basePtr() const; // mapped base + alloc.offset

    size_t descriptorStrideForType(VkDescriptorType type);
    void writeDescriptorRecord(VkDescriptorGetInfoEXT& getInfo, void* dst, size_t dataSize);
    
    RHIVkContext* m_ctx = nullptr;
};

} // namespace rhi::vulkan
