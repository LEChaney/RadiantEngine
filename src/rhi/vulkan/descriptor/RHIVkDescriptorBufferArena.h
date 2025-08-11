#pragma once
#include "rhi/interface/descriptor/RHIDescriptorBufferArena.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi {
class RHIDescriptorSetLayout; 
}

namespace rhi::vulkan {

class RHIVkContext;
class RHIVkBuffer;

class RHIVkDescriptorBufferArena final : public RHIDescriptorBufferArena {
public:
    static UniquePtr<RHIVkDescriptorBufferArena> createUnique(RHIVkContext* ctx, const CreateInfo& ci);

    ~RHIVkDescriptorBufferArena() override;

    RHIVkDescriptorBufferArena(const RHIVkDescriptorBufferArena&) = delete;
    RHIVkDescriptorBufferArena& operator=(const RHIVkDescriptorBufferArena&) = delete;
    RHIVkDescriptorBufferArena(RHIVkDescriptorBufferArena&&) = delete;
    RHIVkDescriptorBufferArena& operator=(RHIVkDescriptorBufferArena&&) = delete;

    RHIDescriptorSetAllocation allocateSet(RHIDescriptorSetLayout* layout, const char* debugName = nullptr) override;
    void resetLinear() override;

    void* mapped() const override { return m_mapped; }
    RHIBuffer* buffer() const override { return m_buffer.get(); }

    bool isValidAddress(void* ptr) const override;
    bool isValidRange(void* ptr, size_t size) const override;

private:
    RHIVkDescriptorBufferArena(RHIVkContext* ctx, const CreateInfo& ci);

    bool allocateRaw(uint64 size, uint64 alignment, uint64& outOffset);

    RHIVkContext* m_ctx = nullptr;
    UniquePtr<RHIBuffer> m_buffer = nullptr;
    void* m_mapped = nullptr;
    uint64 m_size = 0;
    uint64 m_bump = 0;
};

} // namespace rhi::vulkan
