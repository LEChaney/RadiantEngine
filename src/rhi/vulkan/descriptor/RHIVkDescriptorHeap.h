#pragma once
#include "rhi/interface/descriptor/RHIDescriptorHeap.h"
#include "RHIVkDescriptorBuffer.h"

namespace rhi::vulkan {

class RHIVkDescriptorHeap final : public RHIDescriptorHeap {
public:
    static UniquePtr<RHIVkDescriptorHeap> createUnique(RHIVkContext* ctx, RHIDescriptorBuffer* arena, const CreateInfo& ci);

    ~RHIVkDescriptorHeap() override = default;

    RHIVkDescriptorHeap(const RHIVkDescriptorHeap&) = delete;
    RHIVkDescriptorHeap& operator=(const RHIVkDescriptorHeap&) = delete;
    RHIVkDescriptorHeap(RHIVkDescriptorHeap&&) = delete;
    RHIVkDescriptorHeap& operator=(RHIVkDescriptorHeap&&) = delete;

    RHIDescriptorSetLayout* layout() const override { return m_layout; }
    const RHIDescriptorSet& allocation() const override { return m_alloc; }

    uint32 registerSampledImage(RHIImageView* view, RHISampler* sampler) override;
    void updateSampledImage(uint32 index, RHIImageView* view, RHISampler* sampler) override;

private:
    RHIVkDescriptorHeap(RHIVkContext* ctx, RHIDescriptorBuffer* arena, const CreateInfo& ci);

    RHIVkContext* m_ctx = nullptr;
    RHIDescriptorBuffer* m_arena = nullptr;
    CreateInfo m_createInfo{};
    RHIDescriptorSetLayout* m_layout = nullptr; // owned elsewhere (or change to UniquePtr if built here)
    RHIDescriptorSet m_alloc{};       // backing allocation inside arena

    // Simple bitmap for sampled image slots
    Array<uint8> m_sampledBitmap;
};

} // namespace rhi::vulkan
