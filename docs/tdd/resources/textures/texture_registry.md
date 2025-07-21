

# Texture Registry Design Document

## Purpose

Manages texture resources for a scene, including loading, deduplication, and GPU resource management. Provides handles for referencing textures in materials and other systems.

---


## Responsibilities

- Load textures and create GPU resources (images, samplers, views)
- Deduplicate textures per scene
- Provide handles for referencing textures
- Release GPU resources when textures are no longer needed

---

## Registry API and Internals

```cpp
class TextureRegistry {
public:
    TextureRegistry(TextureAllocator* allocator);

    TextureHandle create_texture(const TextureCreateDesc& desc);
    void destroy_texture(TextureHandle handle);
    const Texture& get_texture(TextureHandle handle) const;
    TextureHandle find_texture(const std::string& path) const;

private:
    TextureAllocator* allocator_; // Dependency injected
    SlotMap<Texture> textures;
    // (Optional) path-to-handle map for deduplication
};
```

Typical flow:
- `create_texture` calls allocator to construct the texture, stores it in the registry, and returns the handle.
- `destroy_texture` removes the texture from the registry and calls allocator to destroy it.

---

## Integration with Allocator

- Texture objects should be constructed and destroyed by `TextureAllocator`.
- Registry only stores, looks up, and manages lifetime.

---


## Ownership & Lifetime

- Textures are owned per scene by the registry
- All GPU resources are released when the texture is destroyed or the scene is unloaded

---


## Notes

- Texture deduplication is per scene
- All resource references are by handle; no global sharing
- This document should be updated as the texture registry evolves
