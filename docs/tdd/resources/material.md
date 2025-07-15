
# Material Design Document

## Purpose

Defines a single material instance for a scene, including its parameters, textures, pipeline/layout handles, and descriptor set. Materials are managed by the `MaterialRegistry`.

---

## Responsibilities

- Store all data needed for a material instance (parameters, textures, pipeline/layout handles, descriptor set)
- Provide APIs for creation, update, and destruction via the registry
- Reference textures and pipelines by handle

---


## Data Structures

```cpp
struct Material {
    // Construction Data (CPU side)
    std::vector<TextureHandle> texture_handles;
    MaterialMetadata metadata; // optional
    // PSO Data + Bindings (GPU side)
    PipelineHandle pipeline_handle;
    PipelineLayoutHandle pipeline_layout_handle;
    DescriptorSetHandle descriptor_set;
    MaterialParameters parameters; // Push constants, etc. (May be on CPU or GPU side)
};
```

---

## Ownership & Lifetime

- Materials are owned per scene by the material registry
- All GPU resources are released when the material is destroyed or the scene is unloaded

---

## Notes

- Materials are managed and looked up via the MaterialRegistry
- All resource references are by handle; no global sharing
- This document should be updated as the material system evolves
