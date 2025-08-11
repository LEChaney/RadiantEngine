#pragma once
#include "core/CoreDefs.h"
#include "rhi/interface/core/RHICoreDefs.h"
#include "rhi/interface/descriptor/RHIDescriptorBufferArena.h"
#include "rhi/interface/descriptor/RHIDescriptorWriter.h"

namespace rhi {

class RHIContext;
class RHIImageView;
class RHISampler;
class RHIDescriptorSetLayout;

class RHIDescriptorHeap {
public:
    struct CreateInfo {
        uint32 maxSampledImages = 0;
        uint32 maxStorageImages = 0;
        uint32 maxSamplers = 0;        // optional
        uint32 maxStorageBuffers = 0;  // optional
    };

    virtual ~RHIDescriptorHeap() = default;

    RHIDescriptorHeap(const RHIDescriptorHeap&) = delete;
    RHIDescriptorHeap& operator=(const RHIDescriptorHeap&) = delete;
    RHIDescriptorHeap(RHIDescriptorHeap&&) = delete;
    RHIDescriptorHeap& operator=(RHIDescriptorHeap&&) = delete;

    virtual RHIDescriptorSetLayout* layout() const = 0;
    virtual const RHIDescriptorSetAllocation& allocation() const = 0;

    virtual uint32 registerSampledImage(RHIImageView* view, RHISampler* sampler) = 0;
    virtual void updateSampledImage(uint32 index, RHIImageView* view, RHISampler* sampler) = 0;

protected:
    RHIDescriptorHeap() = default;
};

} // namespace rhi
