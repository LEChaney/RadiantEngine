# Buffer & Image Allocator (RHI)

This document describes the buffer and image allocator abstraction in the Render Hardware Interface (RHI) layer.

## Purpose
- Centralized allocation and deallocation of GPU buffers and images
- Abstracts Vulkan, D3D12, or Metal resource creation details
- Provides a stateless, service-like API for resource management

## Responsibilities
- Allocate and deallocate buffers and images
- Hide low-level API details (VkBuffer, VkImage, memory, etc.)
- Does not track or own higher-level resources (ownership is handled by systems)
- Thread-safe if required

## Example Interface
```cpp
class BufferImageAllocator {
public:
    virtual Buffer* create_buffer(const BufferDesc& desc) = 0;
    virtual void destroy_buffer(Buffer* buffer) = 0;
    virtual Image* create_image(const ImageDesc& desc) = 0;
    virtual void destroy_image(Image* image) = 0;
    virtual ~BufferImageAllocator() = default;
};
```

## Ownership
- Allocator only creates and destroys resources
- Systems track and own allocations

## Related Docs
- See `Resources/mesh_manager.md` and `Resources/texture_manager.md` for higher-level ownership
