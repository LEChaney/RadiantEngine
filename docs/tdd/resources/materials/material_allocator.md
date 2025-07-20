# Material Allocator Design Document

## Purpose

Responsible for constructing and destroying Material and MaterialTemplate objects, including all GPU resource allocation and cleanup.

---

## Responsibilities

- Allocate GPU resources and construct Material/MaterialTemplate objects
- Destroy Material/MaterialTemplate objects and release GPU resources
- Provide API for material/template creation and destruction

---

## Allocator API and Internals

```cpp
class MaterialAllocator {
public:
    Material create_material(const MaterialDesc& desc); // Allocates GPU resources and constructs Material
    void destroy_material(Material& material);           // Releases GPU resources and cleans up Material

    MaterialTemplate create_material_template(const MaterialTemplateDesc& desc); // Allocates GPU resources and constructs MaterialTemplate
    void destroy_material_template(MaterialTemplate& templ);                      // Releases GPU resources and cleans up MaterialTemplate
};
```

---

## Integration with Registry

- MaterialAllocator constructs Material/MaterialTemplate objects, which are then added to MaterialRegistry
- MaterialRegistry only stores and manages their lifetime
