# Pipeline Manager Design Document

## Purpose

The PipelineManager is responsible for creating, caching, and managing all Vulkan graphics and compute pipelines and pipeline layouts used by the renderer. It ensures that each unique pipeline/layout is created only once, provides handles or references for materials and other systems to use, and manages the lifetime and cleanup of pipeline resources.

---

## 1. Responsibilities

- Create and cache Vulkan pipelines and pipeline layouts based on shader code, render pass, pipeline state, and specialization constants.
- Deduplicate pipelines/layouts: ensure that each unique configuration is created only once and shared among all users.
- Provide handles or references for materials and other systems to use pipelines/layouts without duplicating resources.
- Track pipeline usage and manage lifetime, releasing resources when no longer needed (typically at application shutdown or reload).
- Support querying pipeline metadata (shaders, state, etc.) and Vulkan handles (VkPipeline, VkPipelineLayout).

---

## 2. Data Structures

- `PipelineHandle`: Opaque handle or index for referencing pipelines.
- `PipelineLayoutHandle`: Opaque handle or index for referencing pipeline layouts.
- `Pipeline`: Struct containing all Vulkan pipeline state, shader references, and VkPipeline handle.
- `PipelineLayout`: Struct containing VkPipelineLayout and layout metadata.
- Pipeline and layout registries: Maps configuration keys (hash of state, shaders, etc.) to handles and data.

### Descriptor Structs

- `PipelineDesc`: Describes all parameters required to create a Vulkan pipeline. Fields typically include:
  - Shader stages (vertex, fragment, etc.)
  - Render pass and subpass
  - Pipeline layout handle
  - Vertex input state
  - Input assembly state
  - Rasterization state
  - Multisample state
  - Depth/stencil state
  - Color blend state
  - Dynamic state flags
  - Specialization constants (if any)
  - Any additional pipeline creation flags or extensions
- `PipelineLayoutDesc`: Describes the layout of descriptor sets and push constants for a pipeline. Fields typically include:
  - Array of descriptor set layouts (VkDescriptorSetLayout or equivalent handle)
  - Array of push constant ranges
  - Optional: layout flags or metadata for reflection

These descriptor structs are used as keys for deduplication and as the source of truth for pipeline and layout creation. They should be hashable and comparable for efficient registry lookup.

---

## 3. API Overview

- `PipelineHandle requestPipeline(const PipelineDesc& desc);` // Returns a handle to a pipeline, creating or returning an existing one
- `PipelineLayoutHandle requestPipelineLayout(const PipelineLayoutDesc& desc);`
- `const Pipeline& getPipeline(PipelineHandle handle) const;`
- `const PipelineLayout& getPipelineLayout(PipelineLayoutHandle handle) const;`
- `VkPipeline getVkPipeline(PipelineHandle handle) const;`
- `VkPipelineLayout getVkPipelineLayout(PipelineLayoutHandle handle) const;`
- `void releasePipeline(PipelineHandle handle);` // Optional, for explicit cleanup
- `void releasePipelineLayout(PipelineLayoutHandle handle);`

---

## 4. Integration with Other Systems

- **MaterialSystem:**
  - Requests pipelines and layouts from the PipelineManager when creating or updating materials.
  - Stores only handles or references to pipelines/layouts; does not own or duplicate them.
- **Renderer:**
  - Uses pipeline handles in draw data for draw call submission.
- **Resource Allocator:**
  - May be used for pipeline-related GPU allocations (if needed).

---

## 5. Lifetime and Ownership

- Pipelines and layouts are owned and managed by the PipelineManager.
- Materials and other systems reference pipelines/layouts but do not own them.
- Pipelines/layouts are released at application shutdown or when explicitly requested.

---

## 6. Notes

- Pipeline deduplication is based on a unique configuration key (hash of shaders, state, render pass, etc.).
- The PipelineManager may support hot-reloading or dynamic recompilation of pipelines for development.
- The PipelineManager does not perform any rendering; it only manages pipeline and layout resources.

---

## 7. Example Usage

```cpp
// Request a graphics pipeline for a material
PipelineHandle pipelineHandle = pipelineManager.requestPipeline(pipelineDesc);
PipelineLayoutHandle layoutHandle = pipelineManager.requestPipelineLayout(layoutDesc);

// Retrieve Vulkan handles for binding
VkPipeline pipeline = pipelineManager.getVkPipeline(pipelineHandle);
VkPipelineLayout layout = pipelineManager.getVkPipelineLayout(layoutHandle);

// Release when no longer needed (optional, usually at shutdown)
pipelineManager.releasePipeline(pipelineHandle);
pipelineManager.releasePipelineLayout(layoutHandle);
```

---

This document should be updated as the pipeline manager evolves.
