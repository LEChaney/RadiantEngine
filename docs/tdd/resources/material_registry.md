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
    MaterialRegistry(MaterialAllocator* allocator);

    MaterialHandle create_material(const MaterialDesc& desc);
    void destroy_material(MaterialHandle handle);
    MaterialTemplateHandle create_material_template(const MaterialTemplateDesc& desc);
    void destroy_material_template(MaterialTemplateHandle handle);

    const Material& get_material(MaterialHandle handle) const;
    const MaterialTemplate& get_material_template(MaterialTemplateHandle handle) const;

private:
    MaterialAllocator* allocator_; // Dependency injected
    std::unordered_map<MaterialHandle, Material> materials;
    std::unordered_map<MaterialTemplateHandle, MaterialTemplate> templates;
    // (Optional) handle maps for fast lookup
    // (Optional) scene/context info for per-scene ownership
};
```

Typical flow:
- `create_material` calls allocator to construct the material, stores it in the registry, and returns the handle.
- `destroy_material` removes the material from the registry and calls allocator to destroy it.

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
