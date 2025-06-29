# Texture Manager Design Document

## Purpose

The TextureManager is responsible for loading, deduplicating, and managing all texture resources used by materials and other systems. It ensures that each unique texture is loaded only once per scene (or globally, if desired), provides handles for referencing textures, and manages the lifetime and cleanup of texture GPU resources.

---

## 1. Responsibilities

- Load textures from disk or memory and create GPU resources (images, samplers, views).
- Deduplicate textures: ensure that each unique texture is loaded only once per scene (or globally).
- Provide handles or references for materials and other systems to use textures without duplicating resources.
- Track texture usage and manage lifetime, releasing GPU resources when textures are no longer needed.
- Integrate with the Resource Allocator for GPU memory management.
- Support querying texture metadata (dimensions, format, etc.) and GPU handles (VkImage, VkImageView, VkSampler).
- Optionally support reference counting or explicit release APIs for global/shared textures.

---

## 2. Data Structures

- `TextureHandle`: Opaque handle or index for referencing textures.
- `Texture`: Struct containing all GPU resource handles (VkImage, VkImageView, VkSampler), metadata, and reference count (if global/shared).
- Per-scene or global texture registry: Maps texture identifiers (file path, asset ID, etc.) to TextureHandles and Texture data.

### Descriptor Structs

- `TextureLoadDesc`: Describes all parameters required to load a texture. Fields typically include:
  - File path or asset identifier
  - Desired format (e.g., sRGB, UNORM)
  - Mipmapping options (generate, load, or none)
  - Sampler parameters (filtering, address mode, anisotropy, etc.)
  - Usage flags (sampled, storage, etc.)
  - Optional: memory usage hints or residency flags

This struct is used to control how textures are loaded, deduplicated, and created on the GPU. It should be hashable for deduplication and flexible for future extensions.

---

## 3. API Overview

- `TextureHandle loadTexture(Scene* scene, const std::string& path, const TextureLoadDesc& desc);` // Loads or retrieves a texture for a scene
- `void releaseTexture(Scene* scene, TextureHandle handle);` // Releases a texture (decrements ref count or destroys if unused)
- `const Texture& getTexture(Scene* scene, TextureHandle handle) const;` // Retrieves texture data
- `VkImage getVkImage(TextureHandle handle) const;`
- `VkImageView getVkImageView(TextureHandle handle) const;`
- `VkSampler getVkSampler(TextureHandle handle) const;`
- `TextureHandle findTexture(Scene* scene, const std::string& path) const;` // Returns handle if already loaded, else invalid

---

## 4. Integration with Other Systems

- **MaterialSystem:**
  - Requests textures from the TextureManager when creating or updating materials.
  - Stores texture handles in material data; does not own or duplicate texture resources.
- **Resource Allocator:**
  - All GPU resources (images, samplers) for textures are allocated and released via the Resource Allocator.
- **Scene Ownership:**
  - If textures are per-scene, the TextureManager tracks which textures are in use by each scene and releases them on scene unload.
  - If textures are global, reference counting or explicit release APIs are used to manage lifetime.

---

## 5. Lifetime and Ownership

- Textures are owned per scene by default; no cross-scene sharing unless explicitly supported.
- The TextureManager ensures that each unique texture is loaded only once per scene.
- When a scene is unloaded, all textures for that scene are released and GPU resources are destroyed.
- If global sharing is enabled, reference counting or explicit release is required to avoid leaks.

---

## 6. Notes

- Texture deduplication is based on a unique identifier (file path, asset ID, or hash of contents).
- The TextureManager may support loading from disk, memory, or embedded resources.
- Texture metadata (dimensions, format, mip levels) is available for systems that need it.
- The TextureManager does not perform any rendering; it only manages data and resources.

---

## 7. Example Usage

```cpp
// Load or retrieve a texture for a scene
TextureHandle texHandle = textureManager.loadTexture(scene, "albedo.png", loadDesc);

// Retrieve GPU handles for descriptor set creation
VkImage image = textureManager.getVkImage(texHandle);
VkImageView view = textureManager.getVkImageView(texHandle);
VkSampler sampler = textureManager.getVkSampler(texHandle);

// Release the texture when no longer needed
textureManager.releaseTexture(scene, texHandle);
```

---

This document should be updated as the texture manager evolves.
