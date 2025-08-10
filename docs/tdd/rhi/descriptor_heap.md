# RHI Descriptor Heap (Unified Descriptor Buffer + Bindless Resource Table)

Status: Proposal / Draft

## Goals

Provide a unified, modern Vulkan Descriptor Buffer (VK_EXT_descriptor_buffer) based system that:
- Replaces traditional per-descriptor-set allocations with sub-allocations from a single GPU buffer.
- Supports large, indexable arrays ("bindless") for textures / images / buffers.
- Minimizes CPU overhead: no vkUpdateDescriptorSets; all updates are raw memory writes.
- Enables sharing a single descriptor buffer for all regular descriptor sets + global resource table.
- Provides stable 32-bit indices (handles) for shader access.

## Key Concepts

Component | Purpose
--------- | -------
`RHIDescriptorBufferArena` | Owns a large descriptor buffer; sub-allocates aligned slices for descriptor sets / heaps.
`RHIDescriptorSetLayout` | Describes bindings + sizes + internal binding offsets.
`RHIDescriptorSetAllocation` | A view (arena + offset + size + layout) referencing a region inside the arena.
`RHIDescriptorWriter` | CPU helper writing descriptor records directly into mapped memory.
`RHIDescriptorHeap` | A single large set allocation (usually one binding = huge array) offering index-based registration.

## Simplified Data Flow

1. Create one `RHIDescriptorBufferArena` (e.g. 2–8 MB) persistent mapped.
2. Allocate ordinary descriptor sets via `arena->allocateSet(layout)`.
3. Create one global `RHIDescriptorHeap` for bindless resources (sampled images, etc.).
4. Write/update descriptors via `RHIDescriptorWriter` or heap registration APIs.
5. Record command buffer:
   - `cmd->bindDescriptorBuffer(pipelineLayout, setIndex, arena->buffer(), allocation.offset)` per used set.
6. Shader indexes resources directly (e.g. `texture(uTextures[material.albedoIndex], uv)`).

## Core Structures (Pseudo C++)

```cpp
struct RHIDescriptorSetAllocation {
    RHIDescriptorBufferArena* arena = nullptr;
    size_t offset = 0;   // Byte offset in descriptor buffer
    size_t size = 0;     // Allocation size
    RHIDescriptorSetLayout* layout = nullptr;
    bool valid() const { return arena && layout && size > 0; }
};

class RHIDescriptorBufferArena {
public:
    struct CreateInfo { size_t sizeBytes; bool persistentMapped = true; };
    static UniquePtr<RHIDescriptorBufferArena> create(RHIContext*, const CreateInfo&);

    RHIDescriptorSetAllocation allocateSet(RHIDescriptorSetLayout* layout, const char* debugName = nullptr);
    void resetLinear(); // Optional frame-based reset

    void* mapped() const;           // CPU pointer if persistently mapped
    RHIBuffer* buffer() const;      // Underlying GPU buffer
    const RHIDescriptorBufferLimits& limits() const; // alignment & descriptor sizes
};

class RHIDescriptorWriter {
public:
    RHIDescriptorWriter(const RHIDescriptorSetAllocation& alloc);
    void writeSampledImage(uint32_t binding, uint32_t arrayElement, RHIImageView* view, RHISampler* sampler);
    void writeStorageImage(uint32_t binding, uint32_t arrayElement, RHIImageView* view);
    void writeStorageBuffer(uint32_t binding, uint32_t arrayElement, RHIBuffer* buf, size_t offset, size_t range);
    void flush(); // Flush non-coherent memory range
};
```

### Descriptor Heap (Bindless)

```cpp
class RHIDescriptorHeap {
public:
    struct CreateInfo {
        uint32_t maxSampledImages = 65536;
        uint32_t maxStorageImages = 0;
        uint32_t maxSamplers = 0; // optional separate sampler array
        uint32_t maxStorageBuffers = 0;
    };

    static UniquePtr<RHIDescriptorHeap> create(RHIContext* ctx, RHIDescriptorBufferArena* arena, const CreateInfo& ci);

    RHIDescriptorSetLayout* layout() const;             // For pipeline layout construction
    const RHIDescriptorSetAllocation& allocation() const; // For binding

    // Registration returns a stable descriptor index (UINT32_MAX on failure)
    uint32_t registerSampledImage(RHIImageView* view, RHISampler* sampler);
    void updateSampledImage(uint32_t index, RHIImageView* view, RHISampler* sampler);

    // Future: storage images / buffers / samplers
};
```

## Pipeline Layout Integration

Bind the heap like any other set:
```cpp
auto plBuilder = m_context->createPipelineLayoutBuilder();
plBuilder->addSetLayout(descriptorHeap->layout()); // e.g. set = 0
auto pipelineLayout = plBuilder->build();
```

## Command Buffer Binding

```cpp
cmd->bindDescriptorBuffer(pipelineLayout.get(), /*setIndex*/0,
                          descriptorHeap->allocation().arena->buffer(),
                          descriptorHeap->allocation().offset);
```

Multiple ordinary sets can share the same arena; just call bindDescriptorBuffer for each set index.

## Example: Registering Textures

```cpp
auto arena = RHIDescriptorBufferArena::create(ctx, { 2 * 1024 * 1024 });
auto heap  = RHIDescriptorHeap::create(ctx, arena.get(), { .maxSampledImages = 16384 });

uint32_t albedoIdx = heap->registerSampledImage(albedoView, linearSampler);
uint32_t normalIdx = heap->registerSampledImage(normalView, linearSampler);
```

## Shader (GLSL) Example

```glsl
#version 460
#extension GL_EXT_nonuniform_qualifier : enable
layout(set = 0, binding = 0) uniform texture2D uTextures[]; // large descriptor array
layout(set = 0, binding = 1) uniform sampler uSampler;      // (optional separate sampler)

layout(push_constant) uniform MaterialPC { uint albedoIndex; } pc;

layout(location=0) in vec2 vUV;
layout(location=0) out vec4 oColor;

void main() {
    oColor = texture(sampler2D(uTextures[pc.albedoIndex], uSampler), vUV);
}
```

## Compute Shader Clear (Storage Image) Example (from test)

```cpp
auto frame = m_swapchain->acquireNextFrame();
frame.fence->wait(); frame.fence->reset();

// Layout: set0 binding0 = storage image (count=1)
auto setLayoutBuilder = ctx->createDescriptorSetLayoutBuilder();
setLayoutBuilder->addBinding(0, RHIDescriptorType::StorageImage, RHI_SHADER_STAGE_COMPUTE_BIT);
auto setLayout = setLayoutBuilder->build(RHI_SHADER_STAGE_COMPUTE_BIT);

auto plBuilder = ctx->createPipelineLayoutBuilder();
plBuilder->addSetLayout(setLayout.get());
auto pipelineLayout = plBuilder->build();

auto shader = ctx->createShaderModule("shaders/compute_clear.comp.spv");
RHIComputePipelineDescriptor cdesc{ .layout = pipelineLayout.get(), .computeShader = shader.get() };
auto pipeline = ctx->createComputePipeline(cdesc);

auto arena = RHIDescriptorBufferArena::create(ctx, { 128 * 1024 });
auto alloc = arena->allocateSet(setLayout.get(), "ComputeClearSet");
RHIDescriptorWriter(alloc).writeStorageImage(0, 0, frame.imageView).flush();

frame.commandBuffer->reset();
frame.commandBuffer->begin();
frame.commandBuffer->transitionImageLayout(frame.image, RHIImageLayout::General);
frame.commandBuffer->bindComputePipeline(pipeline.get());
frame.commandBuffer->bindDescriptorBuffer(pipelineLayout.get(), 0, arena->buffer(), alloc.offset);
frame.commandBuffer->dispatch((640+7)/8, (480+7)/8, 1);
frame.commandBuffer->transitionImageLayout(frame.image, RHIImageLayout::Present);
frame.commandBuffer->end();

ctx->getGraphicsQueue()->submit({ frame.commandBuffer }, frame.fence, nullptr);
frame.fence->wait();
m_swapchain->present(frame);
```

## Memory & Alignment Notes
- Each set allocation offset must align to `limits().descriptorBufferOffsetAlignment`.
- Binding offsets & descriptor sizes come from Vulkan descriptor buffer queries (cached inside layout construction).
- Persistent mapping avoids per-update allocations; non-coherent memory requires flushing touched ranges.

## Update Patterns
Pattern | Strategy
------- | --------
Per-frame transient sets | Linear bump; `resetLinear()` each frame.
Persistent material sets | Long-lived allocations; reuse & overwrite descriptors.
Bindless heap | Single allocation; in-place writes, optional free list for recycling.

## Error Handling
- Allocation failure returns invalid allocation (`valid()==false`).
- Registration beyond capacity returns `UINT32_MAX`.
- Writers should ASSERT on out-of-range binding / element in debug builds.

## Future Extensions
- Generation counters in handles (index|generation) for safety.
- Separate arenas per descriptor class (optional).
- Compact relocation / defragmentation (copy descriptors + update offsets at bind time).
- Batched `bindDescriptorBuffers` variant for fewer driver calls.

## Summary
`RHIDescriptorHeap` + `RHIDescriptorBufferArena` provide a unified descriptor management path: minimal driver interaction (bind once per set), fast CPU updates (plain memory writes), and scalable resource indexing for modern rendering architectures.
