# Material Allocator Design Document

## Purpose

Responsible for constructing and destroying Material and MaterialInstance objects, including all GPU resource allocation and cleanup.

---

## Responsibilities

- Allocate GPU resources and construct Material/MaterialInstance objects
- Destroy Material/MaterialInstance objects and release GPU resources
- Provide API for material/template creation and destruction

---

## Allocator API and Internals

```cpp
class MaterialAllocator {
public:
    Material create_material(const MaterialDesc& desc); // Allocates GPU resources and constructs Material
    void destroy_material(Material& material);          // Releases GPU resources and cleans up Material

    // Allocates GPU buffer for instance data and constructs MaterialInstance
    MaterialInstance create_material_instance(const MaterialInstanceDesc& desc);
    void destroy_material_instance(MaterialInstance& material_instance); // Releases GPU resources and cleans up MaterialInstance

    // Allocates GPU buffer for parameter collection data and constructs MaterialParameterCollection
    MaterialParameterCollection create_parameter_collection(const MaterialParameterCollectionDesc& desc);
    void destroy_parameter_collection(MaterialParameterCollection& collection); // Releases GPU resources and cleans up MaterialParameterCollection
};
```

---

## Integration with Registry

- MaterialAllocator constructs Material/MaterialInstance objects, which are then added to MaterialRegistry
- MaterialRegistry only stores and manages their lifetime
