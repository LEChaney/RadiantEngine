# DrawData Design Document

## 1. Purpose

Draw data structs serve as the bridge between the CPU-side scene representation and the GPU-side rendering system. Each renderable section of a mesh (not just the whole mesh) is represented by a `MeshDrawData` struct that contains all the data required for a single draw call. This enables efficient rendering of meshes with multiple materials or sections, and decouples scene logic from GPU resource management. Draw data structs are plain data (no virtuals, no inheritance), enabling advanced rendering techniques such as GPU-driven rendering.

---

## 2. Responsibilities

- Mirror the minimal set of data needed for a single draw call from scene objects (e.g., mesh section reference, material, index/vertex buffer, counts, etc.).
- Track changes in the scene (e.g., transform updates, mesh/material changes) and update GPU buffers accordingly (via a manager/system, not the data struct itself).
- Provide a stable, tightly-packed data layout for the renderer to access per-draw-call data for issuing Vulkan draw commands.
- Support efficient, on-demand per-section updates (rather than full scene syncs).
- Enable future GPU-driven workflows (e.g., visibility buffer, meshlet shading, ray tracing).

---

## 3. Structure

- src/renderer/MeshDrawData.h / MeshDrawData.cpp – Data for mesh sections (draw calls).
- src/renderer/LightDrawData.h / LightDrawData.cpp – Data for light objects.
- src/renderer/DrawDataManager.h / DrawDataManager.cpp – Manages all draw data and synchronizes with the scene. Holds all arrays for draw data and transforms.

---

## 4. Key Data Structures

### MeshDrawData
- Represents a single draw call for a mesh section/surface, not the entire mesh.
- **All fields are stored inline for maximum iteration speed; no pointers to material or indirection.**
- Fields:
  - `uint32 indexCount`
  - `uint32 firstIndex`
  - `VkBuffer indexBuffer`
  - `VkDeviceAddress vertexBufferAddress`
  - `VkPipeline pipeline` // Inline, from material assignment
  - `VkPipelineLayout pipelineLayout` // Inline, from material assignment
  - `VkDescriptorSet materialDescriptorSet` // Inline, from material assignment
  - `glm::mat4 transform` // Per mesh section, for rendering (duplicated)
  - (Optional: mesh/section/surface ID for tracking)

### LightDrawData
- Fields:
  - `glm::vec3 position`
  - `glm::vec3 color`
  - `float intensity`
  - (Other light-specific fields as needed)

### DrawDataManager
- Owns and manages all draw data and related arrays:
  - `std::vector<MeshDrawData> meshDrawData;` // Per mesh section
  - `std::vector<LightDrawData> lightDrawData;`
- Methods:
  - `void updateDrawDataForNode(const Node* node)` // Updates all draw data for the node and marks them dirty for GPU sync
  - `void syncTransforms(const std::vector<NodeHandle>& changedNodes, const Scene& scene)` // Synchronize transforms after scene update
  - `const std::vector<MeshDrawData>& getMeshDrawData() const`

---

## 5. Data Flow & Integration with Scene Module

- When a node's transform or mesh/material assignment changes, the scene immediately notifies the DrawDataManager by calling `updateDrawDataForNode(node)`.
- The DrawDataManager marks the corresponding draw data (one per mesh section/draw call) as dirty.
- At a defined point in the frame (e.g., before rendering), the DrawDataManager calls `syncGpuBuffers()`, which updates all dirty draw data GPU buffer regions and clears their dirty flags.
- No full scene sync or traversal is required each frame; only changed draw data are updated.
- The renderer uses the draw data and GPU buffers to issue draw/dispatch calls (e.g., a single visibility pass draw call referencing all mesh draw data).
- The `MeshNode::gather_draw_data` method is responsible for creating `MeshDrawData` for each mesh section, which is then mirrored in the corresponding arrays managed by DrawDataManager.
- **Transforms are stored per mesh section in MeshDrawData.** This enables fast, indirection-free rendering.

---

## 5a. Draw Data and Transform Creation and Lifetime

- Draw data (per mesh section) and transforms (per mesh section) are created immediately when the node is loaded into the scene (e.g., during scene loading or GLTF import).
- The DrawDataManager is notified by the scene module or loader to create draw data and transforms for each relevant mesh and mesh section as soon as they are instantiated and added to the scene graph.
- These arrays are kept alive for the lifetime of their corresponding scene nodes and destroyed only when the node is removed from the scene or the scene is unloaded.
- This matches the lifetime of other GPU resources (meshes, textures, buffers), ensuring all GPU-side data is available for the duration of the scene.
- Scene switching simply updates the renderer's pointer to the new active scene and its draw data; no additional creation or destruction is performed at switch time.

---

## 6. Example Usage

```cpp
// When a node's transform or mesh/material assignment changes:
node->setLocalTransform(newTransform); // updates local transform
scene.updateWorldTransforms(); // propagates to children
cullingSystem.syncTransforms({node}, scene); // update culling system
// ...
drawDataManager.updateDrawDataForNode(node); // marks all draw data for this node as dirty for GPU sync
drawDataManager.syncTransforms({node}, scene); // update draw data transforms

// At the start of the frame (before rendering):
drawDataManager.syncGpuBuffers(); // updates all dirty draw data GPU buffers, clears dirty flags

// Rendering (per mesh section):
for (const auto& drawData : drawDataManager.getMeshDrawData()) {
    // drawData.transform is used directly
    // vkCmdBindPipeline(..., drawData.pipeline, ...);
    // vkCmdBindDescriptorSets(..., drawData.pipelineLayout, drawData.materialDescriptorSet, ...);
    // vkCmdDrawIndexed(..., drawData.indexCount, ..., drawData.firstIndex, ...);
}
```

---

## 7. Future Extensions

- Support for additional draw data types (e.g., decals, reflection probes)
- GPU-driven culling and LOD
- Asynchronous scene/draw data synchronization
- Integration with animation, physics, and scripting systems

---

## 8. References
- [Unreal Engine Render Proxies](https://docs.unrealengine.com/5.0/en-US/RenderingOverview/)
- [GPU-Driven Rendering](https://alextardif.com/GPUDrivenRendering.html)
- [RenderObject struct in current renderer code]

---

This document should be updated as the renderer and scene modules evolve.
