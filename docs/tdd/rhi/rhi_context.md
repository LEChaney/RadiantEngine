# RHI Context

This document describes the RHI context abstraction, following the single responsibility principle.

## Purpose
- Central object for logical device state and top-level GPU access
- Manages devices, queues, and command buffers
- Exposes device-level capabilities and properties

## Responsibilities
- Own/manage device and queues
- Create and destroy command buffers
- Expose device features, limits, and properties
- Do **not** allocate buffers, images, pipelines, or descriptor sets/layouts (see respective allocator docs)
- Do **not** submit commands (see RHI/queue.md)
- Do **not** manage high-level resource ownership (see Resources/material_template.md)


## Example Interface

```cpp
// RHI/RHIContext.h
class RHIContext {
public:
    virtual Queue* get_graphics_queue() = 0;
    virtual CommandBuffer* create_command_buffer() = 0;
    virtual Fence* create_fence() = 0;
    virtual Semaphore* create_semaphore() = 0;
    virtual ~RHIContext() = default;
};
```

## Ownership
- RHIContext owns device and queues
- RHIContext creates and manages command buffers
- Buffer/Image, Pipeline, and DescriptorSet allocation is handled by their respective allocators
- RHIContext creates and manages RHI objects, including descriptor sets/layouts
- High-level resource ownership is handled by Resources

## Related Docs
- See `RHI/command_buffer.md` for command recording
- See `RHI/queue.md` for command submission
- See `RHI/buffer_image_allocator.md` for buffer/image allocation
- See `RHI/pipeline_allocator.md` for pipeline allocation
- See `RHI/descriptor_set_allocator.md` for descriptor set allocation
- See `Resources/material_template.md` for pipeline and descriptor set layout ownership
