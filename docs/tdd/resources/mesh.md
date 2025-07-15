# Mesh & MeshSection Design Document

## Purpose

Defines the data structures for mesh resources, including mesh sections and GPU buffer handles. Meshes are assets managed by the MeshRegistry.

---

## Data Structures

```cpp
struct MeshSection {
    uint32_t first_index;
    uint32_t index_count;
    mat4 bounds_to_mesh;
    std::string name;
    // (Optional: user data, etc.)
};

struct Mesh {
    GPUBuffer vertex_buffer;
    GPUBuffer index_buffer;
    std::vector<MeshSection> sections; // Each section has its own bounds
    std::string name;
    // (Optional: CPU-side data for rebuilds/culling)
};
```

---

## Notes

- Meshes are assets referenced by handle in the MeshRegistry
- MeshSections define sub-parts of a mesh for culling, material assignment, etc.
- This document should be updated as mesh data structures evolve
