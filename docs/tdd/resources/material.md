
# Material Design Document

## Purpose

Defines a single material instance for a scene, including its parameters, textures, pipeline/layout handles, and descriptor set. Materials are managed by the `MaterialRegistry`.

---

## Responsibilities

- Store all data needed for a material instance (parameters, textures, pipeline/layout handles, descriptor set)
- Provide APIs for creation, update, and destruction via the registry
- Reference textures and pipelines by handle

---

## Structure & Internals

- `Material` struct/class holds:
  - `parameters`: Material parameter values (floats, vectors, etc.)
  - `texture_handles`: References to textures (from TextureManager)
  - `pipeline_handle`, `pipeline_layout_handle`: GPU handles for pipeline and layout
  - `descriptor_set`: GPU handle for descriptor set
  - (Optional) metadata for editor integration

---

## Ownership & Lifetime

- Materials are owned per scene by the registry
- All GPU resources are released when the material is destroyed or the scene is unloaded

---

## Notes

- Materials are managed and looked up via the MaterialRegistry
- All resource references are by handle; no global sharing
- This document should be updated as the material system evolves
