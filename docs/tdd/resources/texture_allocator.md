# Texture Allocator Design Document

## Purpose

Responsible for constructing and destroying Texture objects, including all GPU resource allocation and cleanup.

---

## Responsibilities

- Allocate GPU resources and construct Texture objects
- Destroy Texture objects and release GPU resources
- Provide API for texture creation and destruction

---

## Allocator API and Internals

```cpp
class TextureAllocator {
public:
    Texture create_texture(const TextureCreateDesc& desc); // Allocates GPU resources and constructs Texture
    void destroy_texture(Texture& texture);                // Releases GPU resources and cleans up Texture
};
```

---

## Integration with Registry

- TextureAllocator constructs Texture objects, which are then added to TextureRegistry
- TextureRegistry only stores and manages Texture lifetime
