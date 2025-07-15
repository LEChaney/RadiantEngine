# Material Registry Design Document

## Purpose

Manages the collection of both material instances and material templates for a scene (or globally), providing APIs for lookup, creation, and destruction of materials and templates.

---

## Responsibilities

- Store and manage all material instances and templates for a scene
- Provide APIs for creating, destroying, and querying materials and templates
- Ensure unique referencing and efficient lookup

---



## Registry API and Internals

```cpp
class MaterialRegistry {
public:
    void add_material(MaterialHandle handle, Material&& material); // Only stores material, does not allocate
    void add_material_template(MaterialTemplateHandle handle, MaterialTemplate&& templ);
    void remove_material(MaterialHandle handle);                   // Removes and destroys material
    void remove_material_template(MaterialTemplateHandle handle);
    const Material& get_material(MaterialHandle handle) const;
    const MaterialTemplate& get_material_template(MaterialTemplateHandle handle) const;

private:
    std::unordered_map<MaterialHandle, Material> materials;
    std::unordered_map<MaterialTemplateHandle, MaterialTemplate> templates;
    // (Optional) handle maps for fast lookup
    // (Optional) scene/context info for per-scene ownership
};
```

---

## Integration with Allocator

- Material and MaterialTemplate objects should be constructed and destroyed by their respective allocators.
- Registry only stores, looks up, and manages lifetime.

---

## Ownership & Lifetime

- Registry is owned per scene (or globally)
- All materials and templates are destroyed when the registry is destroyed (e.g., on scene unload)

---

## Notes

- The registry provides unified management for both materials and templates
- Decouples storage from instantiation and usage
- This document should be updated as registry features evolve
