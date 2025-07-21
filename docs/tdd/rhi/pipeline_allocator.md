# Pipeline Allocator (RHI)

This document describes the pipeline allocator abstraction in the Render Hardware Interface (RHI) layer.

## Purpose
- Centralized creation and destruction of graphics and compute pipelines
- Abstracts API-specific pipeline creation (VkPipeline, D3D12PipelineState, etc.)
- Optionally manages pipeline cache for reuse and performance

## Responsibilities
- Create and destroy pipelines
- Accept pipeline creation parameters (shaders, states, layouts)
- Optionally manage pipeline cache
- Does not own or manage high-level pipeline usage or sharing

## Example Interface
```cpp
class PipelineAllocator {
public:
    virtual Pipeline* create_graphics_pipeline(const GraphicsPipelineDesc& desc) = 0;
    virtual Pipeline* create_compute_pipeline(const ComputePipelineDesc& desc) = 0;
    virtual void destroy_pipeline(Pipeline* pipeline) = 0;
    virtual ~PipelineAllocator() = default;
};
```

## Ownership
- Allocator only creates and destroys pipelines
- High-level sharing is handled by MaterialTemplate in Resources

## Related Docs
- See `Resources/material_template.md` for pipeline ownership and sharing
