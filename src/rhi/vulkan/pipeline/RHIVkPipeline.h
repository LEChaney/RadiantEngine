#pragma once
#include "rhi/interface/pipeline/RHIPipeline.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVkContext;

class RHIVkPipeline : public RHIPipeline {
public:
    static UniquePtr<RHIVkPipeline> createUniqueGraphics(RHIVkContext* context, const RHIGraphicsPipelineDescriptor& desc);
    static UniquePtr<RHIVkPipeline> createUniqueCompute(RHIVkContext* context, const RHIComputePipelineDescriptor& desc);
    static UniquePtr<RHIVkPipeline> createUniqueRayTracing(RHIVkContext* context, const RHIRayTracingPipelineDescriptor& desc);

    ~RHIVkPipeline() override;
    
    // Non-copyable, non-movable
    RHIVkPipeline(const RHIVkPipeline&) = delete;
    RHIVkPipeline& operator=(const RHIVkPipeline&) = delete;
    RHIVkPipeline(RHIVkPipeline&&) = delete;
    RHIVkPipeline& operator=(RHIVkPipeline&&) = delete;
    
    VkPipeline getVk() const { return m_pipeline; }
    
private:
    RHIVkPipeline(RHIVkContext* context, VkPipeline pipeline);
    
    RHIVkContext* m_context = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

} // namespace rhi::vulkan
