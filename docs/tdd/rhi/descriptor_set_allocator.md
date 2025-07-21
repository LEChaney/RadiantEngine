# Descriptor Set Allocator (RHI)

This document describes the descriptor set allocator abstraction in the Render Hardware Interface (RHI) layer.

## Purpose
- Centralized allocation and freeing of descriptor sets
- Abstracts pool management and Vulkan-specific details
- Provides a simple API for descriptor set allocation

## Responsibilities
- Allocate and free descriptor sets
- Hide pool management and fragmentation
- Does not own or manage high-level sharing or layout ownership

## Example Interface
```cpp
class DescriptorSetAllocator {
public:
    virtual DescriptorSet* allocate_descriptor_set(DescriptorSetLayout* layout) = 0;
    virtual void free_descriptor_set(DescriptorSet* set) = 0;
    virtual ~DescriptorSetAllocator() = default;
};
```

## Ownership
- Allocator only allocates and frees descriptor sets
- High-level sharing and layout ownership is handled by MaterialTemplate in Resources

## Related Docs
- See `Resources/material_template.md` for descriptor set layout sharing
