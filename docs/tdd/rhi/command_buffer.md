# Command Buffer (RHI)

This document describes the command buffer abstraction in the Render Hardware Interface (RHI) layer, following the single responsibility principle.

## Purpose
- Abstract command recording for GPU submission
- Provide API-agnostic interface for recording draw, dispatch, and resource commands

## Responsibilities
- Begin and end command recording
- Record rendering and compute commands (draw, dispatch, bind pipeline, bind resources)
- Manage command buffer lifetime
- Do **not** submit commands to the GPU (see RHI/queue.md)

## Example Interface

```cpp
// RHI/CommandBuffer.h
class CommandBuffer {
public:
    virtual void Begin() = 0;
    virtual void End() = 0;
    virtual void BeginRenderPass(...) = 0;
    virtual void EndRenderPass() = 0;
    virtual void BindPipeline(...) = 0;
    virtual void BindVertexBuffer(...) = 0;
    virtual void BindIndexBuffer(...) = 0;
    virtual void BindDescriptorSet(...) = 0;
    virtual void Draw(...) = 0;
    virtual void Dispatch(...) = 0;
    virtual ~CommandBuffer() = default;
};
```

## Ownership
- Command buffers are created and managed by the RHI context
- Submission is handled by the queue abstraction

## Related Docs
- See `RHI/queue.md` for command submission
- See `RHI/rhi_context.md` for command buffer creation
