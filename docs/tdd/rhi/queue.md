# Queue (RHI)

This document describes the queue abstraction in the Render Hardware Interface (RHI) layer, following the single responsibility principle.

## Purpose
- Abstract GPU command submission
- Provide API-agnostic interface for submitting command buffers to the GPU
- Manage synchronization (fences, semaphores)

## Responsibilities
- Submit recorded command buffers for execution
- Manage synchronization objects (wait/signal semaphores, fences)
- Wait for queue idle
- Do **not** record commands (see RHI/command_buffer.md)

## Example Interface

```cpp
// RHI/Queue.h
class CommandBuffer;
class Fence;

class Queue {
public:
    virtual void Submit(const std::vector<CommandBuffer*>& commandBuffers, Fence* fence = nullptr) = 0;
    virtual void WaitIdle() = 0;
    virtual ~Queue() = default;
};
```

## Ownership
- Queues are created and managed by the RHI context
- Command buffers are submitted to queues for execution

## Related Docs
- See `RHI/command_buffer.md` for command recording
- See `RHI/rhi_context.md` for queue creation
