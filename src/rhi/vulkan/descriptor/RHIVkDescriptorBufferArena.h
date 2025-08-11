#pragma once
#include "rhi/interface/descriptor/RHIDescriptorBufferArena.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi {
class RHIBuffer; 
class RHIDescriptorSetLayout; 
}

namespace rhi::vulkan {

class RHIVkContext;

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

private:
    RHIVkDescriptorBufferArena(RHIVkContext* ctx, const CreateInfo& ci);

    bool allocateRaw(size_t size, size_t alignment, size_t& outOffset);

    RHIVkContext* m_ctx = nullptr;
    CreateInfo m_createInfo{};
    UniquePtr<RHIBuffer> m_buffer; // RHI buffer abstraction (will be Vulkan buffer internally)
    void* m_mapped = nullptr;      // persistent mapped pointer (if enabled)
    size_t m_size = 0;
    size_t m_bump = 0;             // linear bump offset
};

} // namespace rhi::vulkan
