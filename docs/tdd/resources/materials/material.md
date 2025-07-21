## Material Class

### Purpose
The `Material` class represents a material type, holding references to its pipeline, shader, and reflection info. It is responsible for providing the layout and metadata required for material instances and for integration with the material registry and descriptor sets.

### Responsibilities
- Stores pipeline and shader references for rendering.
- Holds a pointer to the `MaterialReflection` info for parameter validation and layout.
- Provides access to reflection info for instances and registries.
- May provide access to descriptor set layouts and pipeline handles.

### Key API
```cpp
class Material {
public:
    const MaterialReflection* GetReflection() const;
    VkPipeline GetPipeline() const;
    // ...other low-level info

private:
    VkPipeline m_pipeline; // Vulkan pipeline handle
    MaterialReflection m_reflection; // Reflection info for parameter offsets and validation
    // Other internal state as needed
};
```

### Integration
- Used by `MaterialInstance` to validate and pack parameters.
- Registered and managed by `MaterialRegistry`.
- Interacts with the RHI layer for pipeline and descriptor set management.

### Example Usage
```cpp
Material* material = materialRegistry->GetMaterial(handle);
const MaterialReflection* refl = material->GetReflection();
VkPipeline pipeline = material->GetPipeline();
```
