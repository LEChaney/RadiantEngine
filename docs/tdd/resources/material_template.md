
# Material Template Design Document

## Purpose

Defines a reusable template for materials, specifying shared shader code, descriptor set layout, and default parameters. Material templates allow rapid creation of similar materials with minimal duplication.

---

## Responsibilities

- Specify shader(s), descriptor set layout, and default parameters for a family of materials.
- Provide a simple API for instantiating a material from a template, with optional parameter overrides.

---

## Structure & Internals

- `MaterialTemplate` struct/class is split into:
  - **Construction Data (CPU side):**
    - `name` or `id`: Unique identifier for the template
    - `shader_refs`: References to shader modules (vertex, fragment, etc.)
    - `descriptor_set_layout_desc`: Descriptor set layout description (not GPU handles)
    - `default_parameters`: Default values for material parameters (e.g., colors, floats, textures)
    - `metadata`: Optional authoring info, tags, etc.
  - **PSO Data (GPU side):**
    - `pipeline_handle`: GPU handle for pipeline, returned by pipeline allocator
    - `pipeline_layout_handle`: GPU handle for pipeline layout, returned by pipeline allocator
    - `descriptor_set_layout_handle`: GPU handle for descriptor set layout, returned by pipeline allocator

- Construction data is used to request PSO data from the pipeline allocator. PSO data is cached and returned for rendering and descriptor set creation.
- Templates are referenced by name, ID, or asset path.

---

## Example API

```cpp
// Create a template
MaterialTemplateHandle create_material_template(const MaterialTemplateDesc& desc);

// Instantiate a material from a template
MaterialHandle instantiate_material(MaterialTemplateHandle template, const MaterialDesc& overrides);

// Query template
const MaterialTemplate& get_material_template(MaterialTemplateHandle handle) const;
```

---

## Ownership & Lifetime

- Templates are owned per scene or globally.
- Materials instantiated from templates are managed by the MaterialSystem.

---

## Notes

- Templates simplify material creation and reduce duplication.
- This document should be updated as template features evolve.
