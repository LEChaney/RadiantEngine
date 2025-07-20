# Texture Class Design Document

## Purpose

Represents a single texture resource in the renderer, encapsulating GPU image, view, and sampler objects. Provides API for accessing texture properties and handles for bindless usage in materials and shaders.

---

## Responsibilities

- Encapsulate GPU image, image view, and sampler
- Store metadata (dimensions, format, mip levels, etc.)
- Provide accessors for GPU handles and metadata
- Support bindless indexing for descriptor arrays
- Integrate with TextureRegistry and TextureAllocator for creation and destruction

---

## Texture API and Internals

```cpp
class Texture {
public:
    VkImage image() const;
    VkImageView view() const;
    VkSampler sampler() const;
    uint32_t bindless_index() const; // Index in bindless descriptor array
    uint32_t width() const;
    uint32_t height() const;
    VkFormat format() const;
    uint32_t mip_levels() const;
    // ...additional metadata accessors...
private:
    VkImage image_;
    VkImageView view_;
    VkSampler sampler_;
    uint32_t bindless_index_;
    uint32_t width_;
    uint32_t height_;
    VkFormat format_;
    uint32_t mip_levels_;
    // ...other metadata...
};
```

---

## Integration with Registry and Allocator

- Texture objects are constructed and destroyed by `TextureAllocator`
- TextureRegistry manages lifetime and deduplication
- Bindless index is assigned by registry for use in descriptor arrays

---

## Notes

- Texture class is a lightweight wrapper; does not own GPU resources directly
- All resource management is handled by allocator and registry
- Bindless index enables fast lookup in shaders
- This document should be updated as the texture class evolves
