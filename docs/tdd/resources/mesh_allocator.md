# Mesh Allocator Design Document

## Purpose

Responsible for constructing and destroying Mesh objects, including all GPU resource allocation and cleanup.

---

## Responsibilities

- Allocate GPU buffers and construct Mesh objects
- Destroy Mesh objects and release GPU resources
- Provide API for mesh creation and destruction

---

## Allocator API and Internals

```cpp
class MeshAllocator {
public:
    Mesh create_mesh(const MeshSource& src); // Allocates buffers and constructs Mesh
    void destroy_mesh(Mesh& mesh);           // Releases buffers and cleans up Mesh
};
```

---

## Integration with Registry

- MeshAllocator constructs Mesh objects, which are then added to MeshRegistry
- MeshRegistry only stores and manages Mesh lifetime
