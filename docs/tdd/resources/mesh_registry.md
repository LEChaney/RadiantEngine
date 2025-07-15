

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
    void add_mesh(MeshHandle handle, Mesh&& mesh); // Only stores mesh, does not allocate
    void remove_mesh(MeshHandle handle);           // Removes and destroys mesh
    const Mesh& get_mesh(MeshHandle handle) const;
    const std::vector<MeshSection>& get_sections(MeshHandle handle) const;

private:
    std::unordered_map<MeshHandle, Mesh> meshes;
    // (Optional) handle maps for fast lookup
    // (Optional) scene/context info for per-scene ownership
};
```

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

