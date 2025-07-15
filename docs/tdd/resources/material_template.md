
# Material Template Design Document

## Purpose

Defines a reusable template for materials, specifying shared shader code, descriptor set layout, and default parameters. Material templates allow rapid creation of similar materials with minimal duplication.

---

## Responsibilities

- Specify shader(s), descriptor set layout, and default parameters for a family of materials.

---

## Data Structures

```cpp
struct MaterialTemplate {
    // Construction Data (CPU side)
    std::string name;
    ShaderRefs shader_refs;
    DescriptorSetLayoutDesc descriptor_set_layout_desc;
    MaterialParameters default_parameters;
    MaterialTemplateMetadata metadata;

    // PSO Data (GPU side)
    PipelineHandle pipeline_handle;
    PipelineLayoutHandle pipeline_layout_handle;
    DescriptorSetLayoutHandle descriptor_set_layout_handle;
};
```

---

## Ownership & Lifetime

- Templates are owned per scene by the material registry.
- Materials instantiated from templates are also managed by the material registry.

---

## Notes

- Templates simplify material creation and reduce duplication.
- This document should be updated as template features evolve.
