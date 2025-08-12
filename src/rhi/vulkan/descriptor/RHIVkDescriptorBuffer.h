#pragma once
#include "rhi/interface/descriptor/RHIDescriptorBuffer.h"
#include "rhi/vulkan/buffer/RHIVkBuffer.h"

namespace rhi {
class RHIDescriptorSetLayout;
}

namespace rhi::vulkan {

class RHIVkContext;
class RHIVkBuffer;

class RHIVkDescriptorBuffer final : public RHIDescriptorBuffer {
public:
    static UniquePtr<RHIVkDescriptorBuffer> createUnique(RHIVkContext* ctx, const CreateInfo& ci);

    ~RHIVkDescriptorBuffer() override;

    RHIVkDescriptorBuffer(const RHIVkDescriptorBuffer&) = delete;
    RHIVkDescriptorBuffer& operator=(const RHIVkDescriptorBuffer&) = delete;
    RHIVkDescriptorBuffer(RHIVkDescriptorBuffer&&) = delete;
    RHIVkDescriptorBuffer& operator=(RHIVkDescriptorBuffer&&) = delete;

    RHIDescriptorSet allocateSet(RHIDescriptorSetLayout* layout,
        const std::string& debugName = "") override;
    void resetLinear() override;

private:
    RHIVkDescriptorBuffer(RHIVkContext* ctx, const CreateInfo& ci);

    bool allocateRaw(uint64 size, uint64 alignment, uint64& outOffset);

    RHIVkContext* m_ctx = nullptr;
    void* m_mapped = nullptr;
    uint64 m_bump = 0;
};

} // namespace rhi::vulkan
