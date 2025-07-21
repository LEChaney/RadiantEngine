# Descriptor Set & Descriptor Set Layout (RHI)

This document describes the descriptor set and descriptor set layout abstractions in the Render Hardware Interface (RHI) layer, following the single responsibility principle.

## Purpose
- Abstract resource binding for shaders
- Provide API-agnostic interfaces for descriptor sets and layouts

## Responsibilities
- Descriptor Set Layout: defines the structure of resource bindings (types, slots)
- Descriptor Set: holds actual resource bindings (buffers, images, samplers)
- Manage creation, update, and destruction of descriptor sets and layouts
- Do **not** manage high-level sharing or ownership (see Resources/material_template.md)

## Example Interfaces

```cpp
// RHI/DescriptorSetLayout.h
class DescriptorSetLayout {
public:
    virtual ~DescriptorSetLayout() = default;
    // API-agnostic descriptor set layout interface
};

// RHI/DescriptorSet.h
class DescriptorSet {
public:
    virtual void update(const DescriptorUpdateInfo& info) = 0;
    virtual ~DescriptorSet() = default;
};
```

## Ownership
- Descriptor sets and layouts are created and managed by the RHI context
- High-level sharing and reuse is handled by MaterialTemplate in Resources

## Related Docs
- See `Resources/material_template.md` for descriptor set layout sharing
- See `RHI/rhi_context.md` for descriptor set/layout allocation
