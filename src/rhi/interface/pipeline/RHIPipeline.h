#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"

namespace RHI {

class RHIPipelineLayout;
class RHIShaderModule;

struct RHIGraphicsPipelineDescriptor {
    RHIPipelineLayout* layout = nullptr;
    // Optional task shader. When provided, pipeline stages are: task -> mesh -> fragment.
    RHIShaderModule* taskShader = nullptr; // may be null
    RHIShaderModule* meshShader = nullptr;
    RHIShaderModule* fragmentShader = nullptr;
    RHIFormat colorFormat = RHIFormat::RHI_FORMAT_UNDEFINED;
    RHIFormat depthFormat = RHIFormat::RHI_FORMAT_UNDEFINED;
    RHIFormat stencilFormat = RHIFormat::RHI_FORMAT_UNDEFINED;
    bool depthWriteEnable = true;
    bool depthTestEnable = true;
    // Add pipeline descriptor fields as needed
};

struct RHIComputePipelineDescriptor {
    RHIPipelineLayout* layout = nullptr;
    RHIShaderModule* computeShader = nullptr;
    // Add compute pipeline descriptor fields as needed
};

struct RHIRayTracingPipelineDescriptor {
    RHIPipelineLayout* layout = nullptr;
    RHIShaderModule* raygenShader = nullptr;
    RHIShaderModule* missShader = nullptr;
    Array<RHIShaderModule*> hitShaders;
    Array<RHIShaderModule*> callableShaders;
    // Add ray tracing pipeline descriptor fields as needed
};

class RHIPipeline {
public:
    virtual ~RHIPipeline() = default;
    RHIPipeline(const RHIPipeline&) = delete;
    RHIPipeline& operator=(const RHIPipeline&) = delete;
    RHIPipeline(RHIPipeline&&) = delete;
    RHIPipeline& operator=(RHIPipeline&&) = delete;
protected:
    RHIPipeline() = default;
};

} // namespace rhi
