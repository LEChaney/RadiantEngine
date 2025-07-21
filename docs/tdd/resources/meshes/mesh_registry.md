

# Mesh Registry Design Document

## Purpose

Manages the collection of mesh resources for a scene, providing APIs for lookup, creation, and destruction of meshes.

---

## Responsibilities

- Store and manage all mesh resources for a scene
- Provide APIs for creating, destroying, and querying meshes
- Integrate with Resource Allocator for buffer management

---

## Registry API and Internals

```cpp
class MeshRegistry {
public:
    MeshRegistry(MeshAllocator* allocator);

    MeshHandle create_mesh(const MeshSource& src);
    void destroy_mesh(MeshHandle handle);
    const Mesh& get_mesh(MeshHandle handle) const;
    const std::vector<MeshSection>& get_sections(MeshHandle handle) const;

private:
    MeshAllocator* allocator_; // Dependency injected
    SlotMap<Mesh> meshes;
    // (Optional) handle maps for fast lookup
    // (Optional) scene/context info for per-scene ownership
};
```

Typical flow:
- `create_mesh` calls allocator to construct the mesh, stores it in the registry, and returns the handle.
- `destroy_mesh` removes the mesh from the registry and calls allocator to destroy it.


---

## Integration with Allocator

- Mesh objects should be constructed and destroyed by `MeshAllocator`.
- Registry only stores, looks up, and manages lifetime.

---

## Ownership & Lifetime

- Registry is owned per scene
- All meshes are destroyed when the registry is destroyed (e.g., on scene unload)

---

## Notes

- The registry provides unified management for all meshes
- Decouples storage from instantiation and usage
- This document should be updated as registry features evolve

