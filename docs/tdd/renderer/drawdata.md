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
- Provide APIs for clearing all draw data and repopulating from a new scene.

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
  - `glm::mat4 worldTransform` // Per mesh section, for rendering (duplicated)
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
  - `std::unordered_map<NodeHandle, std::vector<size_t>> nodeToDrawIndices;` // Maps scene nodes to their draw data indices
- Methods:
  - `void clearAllData();` // Clears all draw data and mappings.
  - `void populateFromScene(Scene* scene);` // Populates all draw data and mappings from the given scene.
  - `void syncTransforms(const std::vector<NodeHandle>& changedNodes)` // Synchronize transforms after scene update. Also marks draw data dirty for GPU sync
  - `const std::vector<MeshDrawData>& getMeshDrawData() const`
  - `void createDrawDataForMeshInstance(const MeshInstance& meshInstance);` // Creates MeshDrawData for each mesh section in the given mesh instance

---

## 5. Data Flow & Integration

- DrawDataManager tracks per-mesh-section draw data and marks only changed entries as dirty.
- Before rendering, `syncGpuBuffers()` updates all dirty regions in GPU buffers and resets their dirty flags.
- Only modified draw data are synchronized each frame—no full scene traversal is required.
- The renderer issues draw calls directly from the flat arrays managed by DrawDataManager.
- Per-section transforms are stored directly in `MeshDrawData` for fast, indirection-free access.
- When switching scenes, DrawDataManager clears all data and repopulates from the new scene.

---

## 5a. Draw Data Lifecycle

- Draw data are created when a scene is activated or when a mesh instance is added at runtime.
- Draw data persist for the lifetime of its corresponding mesh instance and are destroyed when the mesh instance is destroyed, or the scene is unloaded.
- This ensures GPU resources are always in sync with the active scene.
- On scene switch, DrawDataManager clears and repopulates all draw data and mappings.

---

### Node-to-Draw Index Mapping

DrawDataManager maintains a mapping from scene nodes to their associated draw indices (one per mesh section). This enables efficient, targeted updates:

- When a node's transform changes, DrawDataManager uses this mapping to update only the relevant draw data and culling entries.
- The mapping is updated whenever mesh instances are added, removed, or when the scene is loaded/unloaded.
- Only DrawDataManager owns and maintains this mapping; other systems query it as needed.

**Example:**
```cpp
for (NodeHandle node : changedNodes) {
    for (size_t drawIdx : drawDataManager.getDrawIndicesForNode(node)) {
        // Update transform in DrawDataManager and CullingSystem
    }
}
```

This design enables flat, cache-friendly, and parallel updates to per-draw data after scene changes.

---

## 6. Example Usage

```cpp
// When switching scenes:
drawDataManager.clearAllData();
drawDataManager.populateFromScene(newScene); // Repopulates all draw data from the new scene

// When a node's transform or mesh/material assignment changes:
node->setLocalTransform(newTransform); // updates local transform
scene.updateWorldTransforms(); // propagates to children
cullingSystem.syncTransforms({node}); // update culling system
// ...
drawDataManager.syncTransforms({node}); // update draw data transforms and add to dirty queue for gpu sync

// When dynamically adding a mesh instance to the active scene:
MeshInstance& meshInstance = meshSystem.addMeshInstance(activeScene, node, mesh, matSet); // Automatically creates draw data for the new mesh instance if the scene is active
// (Internally, this will call drawDataManager.createDrawDataForMeshInstance(meshInstance);)

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
