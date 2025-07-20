# MaterialInstance Registry Design Document

## Purpose

Manages the collection of both material and material instances for a scene (or globally), providing APIs for lookup, creation, and destruction of materials and material instances.

Additionally, the registry integrates with bindless GPU resource management, reflection-driven validation, and descriptor set allocation for modern Vulkan-based rendering.

---

## Responsibilities

- Store and manage all materials and material instances for a scene
- Provide APIs for creating, destroying, and querying materials and material instances
- Ensure unique referencing and efficient lookup
- Integrate with bindless descriptor sets and buffer device address management
- Provide reflection info for validation and parameter packing
- Coordinate with TextureRegistry for bindless texture indices
- Manage per-material buffer addresses for GPU access

---

## Registry API and Internals

```cpp
class MaterialRegistry {
public:
    MaterialRegistry(MaterialAllocator* allocator);

    MaterialHandle create_material(const MaterialDesc& desc);
    void destroy_material(MaterialHandle handle);
    MaterialInstanceHandle create_material_instance(const MaterialInstanceDesc& desc);
    void destroy_material_instance(MaterialInstanceHandle handle);

    // MaterialParameterCollection management
    MaterialParameterCollectionHandle create_parameter_collection(const MaterialParameterCollectionDesc& desc);
    void destroy_parameter_collection(MaterialParameterCollectionHandle handle);
    const MaterialParameterCollection& get_parameter_collection(MaterialParameterCollectionHandle handle) const;

    const Material& get_material(MaterialHandle handle) const;
    const MaterialInstance& get_material_instance(MaterialInstanceHandle handle) const;

    // Bindless GPU integration
    VkDeviceAddress get_material_address(MaterialInstanceHandle handle) const;
    const MaterialReflection* get_reflection(MaterialHandle handle) const;
    // Returns the globally bound descriptor set containing bindless resources.
    // This set typically includes:
    //   - A buffer of material data pointers (device addresses), one per instance/material
    //   - Bindless texture arrays (all registered textures)
    // Shaders use this set to access per-instance material data and textures via indices or addresses.
    // The set is intended to be bound once per frame or pass, enabling flexible, scalable material access for both rasterization and ray tracing pipelines.
    VkDescriptorSet get_bindless_descriptor_set() const;

private:
    MaterialAllocator* allocator_; // Dependency injected
    std::unordered_map<MaterialInstanceHandle, MaterialInstance> material_instances;
    std::unordered_map<MaterialHandle, Material> materials;
    std::unordered_map<MaterialParameterCollectionHandle, MaterialParameterCollection> parameter_collections;
    // (Optional) handle maps for fast lookup
    // (Optional) scene/context info for per-scene ownership
};
```

Typical flow:
- `create_material` calls allocator to construct the material, stores it in the registry, and returns the handle.
- `destroy_material` removes the material from the registry and calls allocator to destroy it.
- Reflection info is loaded and stored per material for validation and parameter packing.
- Buffer device addresses are managed for each material instance for bindless GPU access.
- Descriptor sets are allocated and managed for bindless texture/material access.

---

## Integration with Allocator

- MaterialInstance and Material objects should be constructed and destroyed by their respective allocators.
- Registry only stores, looks up, and manages lifetime.
- Registry provides GPU resource handles (device addresses, descriptor sets) to other systems.
- Registry coordinates with TextureRegistry for texture indices and bindless arrays.

---

## Ownership & Lifetime

- Registry is owned per scene (or globally)
- All materials and material instances are destroyed when the registry is destroyed (e.g., on scene unload)
- Registry ensures proper cleanup of GPU resources and descriptor sets.

---

## Notes

- The registry provides unified management for both materials and material instances.
- Decouples storage from instantiation and usage
- Integrates with bindless material system for modern Vulkan rendering
- Reflection-driven validation and buffer packing
- Descriptor set and buffer device address management for GPU access
- This document should be updated as registry features evolve
