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
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void begin_render_pass(...) = 0;
    virtual void end_render_pass() = 0;
    virtual void bind_pipeline(...) = 0;
    virtual void bind_vertex_buffer(...) = 0;
    virtual void bind_index_buffer(...) = 0;
    virtual void bind_descriptor_set(...) = 0;
    virtual void draw(...) = 0;
    virtual void dispatch(...) = 0;
    virtual ~CommandBuffer() = default;
};
```

## Ownership
- Command buffers are created and managed by the RHI context
- Submission is handled by the queue abstraction

## Related Docs
- See `RHI/queue.md` for command submission
- See `RHI/rhi_context.md` for command buffer creation
