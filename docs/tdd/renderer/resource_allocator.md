# Resource Allocator Design Document

## Purpose

The Resource Allocator is responsible for low-level GPU resource allocation and deallocation (buffers, images, etc.) in the renderer. It provides a stateless, service-like API for Vulkan resource management, but does not own, track, or manage the lifetime of higher-level resources (such as meshes, materials, or scene data). Ownership and lifetime management are handled by higher-level systems (see [Scene Resource Ownership & Lifetime](scene_resource_ownership.md)).

---

## 1. Responsibilities

- **Allocate and deallocate GPU resources** (buffers, images, memory, etc.) on request from higher-level systems.
- **Abstract Vulkan allocation details** (VkBuffer, VkImage, VkDeviceMemory, VMA, etc.) behind a simple API.
- **Stateless operation:** Does not track or own resources after allocation; callers are responsible for tracking and releasing resources.
- **No resource lifetime management:** Does not perform reference counting, pooling, or garbage collection.
- **Thread safety:** Should be safe to use from multiple systems/threads if required.

---

## 2. API Overview

### Typical API (C++-like pseudocode)

```cpp
// Buffer allocation
struct BufferCreateInfo {
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VmaMemoryUsage memoryUsage;
    // ... other Vulkan/VMA options ...
};

struct GPUBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VkDeviceSize size;
};

GPUBuffer createBuffer(const BufferCreateInfo& info);
void destroyBuffer(const GPUBuffer& buffer);

// Image allocation
struct ImageCreateInfo {
    VkExtent3D extent;
    VkFormat format;
    VkImageUsageFlags usage;
    VmaMemoryUsage memoryUsage;
    // ... other Vulkan/VMA options ...
};

struct GPUImage {
    VkImage image;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format;
};

GPUImage createImage(const ImageCreateInfo& info);
void destroyImage(const GPUImage& image);

// (Optional) Map/unmap memory
void* mapMemory(const GPUBuffer& buffer);
void unmapMemory(const GPUBuffer& buffer);
```

- All returned handles are plain structs; the allocator does not retain ownership or track them.
- Callers must call `destroyBuffer`/`destroyImage` when the resource is no longer needed.

---

## 3. Usage Pattern

- **Systems (MeshSystem, MaterialSystem, etc.)** request buffer/image allocations for their scene resources via the Resource Allocator.
- **Systems track all allocations** they make, associating them with the owning scene and resource.
- **On scene unload**, systems call the appropriate destroy methods for all resources allocated for that scene.
- **No cross-scene sharing:** Each scene's resources are allocated and destroyed independently.
- **No internal resource tracking:** The allocator does not know which scene or system owns a resource.

---

## 4. Integration with Vulkan and VMA

- The Resource Allocator typically wraps [Vulkan Memory Allocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) for efficient memory management.
- It may also provide helpers for staging buffer uploads, memory barriers, and resource transitions, but these are stateless utilities.
- The allocator is initialized with Vulkan device/context handles and a VMA allocator instance.

---

## 5. Example Usage

```cpp
// MeshSystem allocates a vertex buffer for a mesh:
BufferCreateInfo info = { ... };
GPUBuffer vertexBuffer = resourceAllocator.createBuffer(info);
// ...
resourceAllocator.destroyBuffer(vertexBuffer);

// MaterialSystem allocates a texture image:
ImageCreateInfo imgInfo = { ... };
GPUImage albedoImage = resourceAllocator.createImage(imgInfo);
// ...
resourceAllocator.destroyImage(albedoImage);
```

---

## 6. Lifetime and Ownership

- **Resource Allocator:** Only allocates and deallocates; does not own or track resources.
- **Systems:** Own and track all allocations for their scene resources; responsible for cleanup.
- **Scene:** Owns all resources for its nodes; on unload, all resources are released via the systems.

---

## 7. Error Handling and Validation

- Allocation failures (e.g., out of memory) are reported via return codes or exceptions.
- The allocator may provide debug utilities for tracking leaks or reporting usage, but does not enforce ownership.

---

## 8. Summary

- The Resource Allocator is a stateless, service-like utility for Vulkan resource allocation.
- It does not own, track, or manage resource lifetimes; all tracking is done by higher-level systems.
- All resources are owned per scene, and are allocated/destroyed via the systems on scene load/unload.
- This design ensures clear ownership, robust cleanup, and efficient resource usage.

---

This document should be updated as the allocator or resource model evolves.
