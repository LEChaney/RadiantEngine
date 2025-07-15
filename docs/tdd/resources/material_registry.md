# Material Registry Design Document

## Purpose

Manages the collection of both material instances and material templates for a scene (or globally), providing APIs for lookup, creation, and destruction of materials and templates.

---

## Responsibilities

- Store and manage all material instances and templates for a scene
- Provide APIs for creating, destroying, and querying materials and templates
- Ensure unique referencing and efficient lookup

---

## Structure & Internals

- `MaterialRegistry` holds:
  - `materials`: Registry of `Material` objects (instances)
  - `templates`: Registry of `MaterialTemplate` objects
  - (Optional) handle maps for fast lookup
  - (Optional) scene/context info for per-scene ownership

---

## Example API

```cpp
// Create a new material template
MaterialTemplateHandle create_material_template(const MaterialTemplateDesc& desc);

// Create a new material instance
MaterialHandle create_material(const MaterialDesc& desc);

// Destroy a material or template
void destroy_material(MaterialHandle handle);
void destroy_material_template(MaterialTemplateHandle handle);

// Query material or template
const Material& get_material(MaterialHandle handle) const;
const MaterialTemplate& get_material_template(MaterialTemplateHandle handle) const;
```

---

## Ownership & Lifetime

- Registry is owned per scene (or globally)
- All materials and templates are destroyed when the registry is destroyed (e.g., on scene unload)

---

## Notes

- The registry provides unified management for both materials and templates
- Decouples storage from instantiation and usage
- This document should be updated as registry features evolve
