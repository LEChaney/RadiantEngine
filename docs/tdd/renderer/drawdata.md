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

### MaterialDrawData
- Represents the minimal, immutable set of data required for a draw call from a material: pipeline, pipeline layout, and descriptor set.
- **Does not reference high-level material parameters, textures, or buffers directly.**
- Fields:
  - `VkPipeline pipeline`
  - `VkPipelineLayout pipelineLayout`
  - `VkDescriptorSet descriptorSet`

### MeshDrawData
- Represents a single draw call for a mesh section/surface, not the entire mesh.
- **All fields are stored inline for maximum iteration speed; no pointers to material or indirection.**
- Fields:
  - `uint32 indexCount`
  - `uint32 firstIndex`
  - `VkBuffer indexBuffer`
  - `VkDeviceAddress vertexBufferAddress`
  - `MaterialDrawData materialDrawData` // Inline struct, see above
  - `glm::mat4 worldTransform` // Per mesh section, for rendering (duplicated)
  - (Optional: mesh/section/surface ID for tracking)

### LightDrawData
- Fields:
  - `glm::vec3 position`
  - `glm::vec3 color`
  - `float intensity`
  - (Other light-specific fields as needed)

### MeshDrawCullData
- Stores the oriented bounding box (OBB) for a mesh section in world space, used for culling.
- Fields:
  - `glm::mat4 boundsToWorld` // OBB transform in world space (updated when node/world transform changes)

### MeshDrawBoundsData
- Stores the OBB for a mesh section in mesh local space (static, only needed for resync or mesh deformation).
- Fields:
  - `glm::mat4 boundsToMesh` // OBB transform in mesh local space

### DrawDataManager
Owns and manages all per-draw arrays required for rendering and culling:
  - `std::vector<MeshDrawData> meshDrawData;` // Per mesh section
  - `std::vector<MeshDrawCullData> meshDrawCullData;` // Per mesh section, for culling (was previously owned by CullingSystem)
  - `std::vector<MeshDrawBoundsData> meshDrawBoundsData;` // Per mesh section, for culling (was previously owned by CullingSystem)
  - `std::vector<LightDrawData> lightDrawData;`
  - `std::unordered_map<NodeHandle, std::vector<size_t>> nodeToDrawIndices;` // Maps scene nodes to their draw data indices
Methods:
  - `void clearAllData();` // Clears all draw/culling data and mappings.
  - `void populateFromScene(Scene* scene);` // Populates all draw/culling data and mappings from the given scene.
  - `const std::vector<MeshDrawData>& getMeshDrawData() const`
  - `const std::vector<MeshDrawCullData>& getMeshDrawCullData() const`
  - `const std::vector<MeshDrawBoundsData>& getMeshDrawBoundsData() const`
  - `void createDrawDataForMeshInstance(const MeshInstance& meshInstance);` // Creates MeshDrawData and culling data for each mesh section in the given mesh instance
  - `void onTransformsFinalized(const std::vector<NodeHandle>& changedNodes);` // Observer pattern callback: updates transforms and culling data for changed nodes

---

## 5. Data Flow & Integration

- DrawDataManager tracks per-mesh-section draw data and marks only changed entries as dirty.
- Before rendering, `syncGpuBuffers()` updates all dirty regions in GPU buffers and resets their dirty flags.
- Only modified draw data are synchronized each frame—no full scene traversal is required.
- The renderer issues draw calls directly from the flat arrays managed by DrawDataManager.
- Per-section transforms are stored directly in `MeshDrawData` for fast, indirection-free access.
- When switching scenes, DrawDataManager clears all data and repopulates from the new scene.
- **Observer Pattern:** DrawDataManager acts as an observer to MeshSystem and MaterialSystem, updating draw data in response to notifications about mesh instance or material changes. See [Observer Pattern](../core/observer_pattern.md) for details.
- **Testability:** Observer registration is explicit and can be performed with either real or mock systems, supporting robust unit and integration testing.

---

## 5a. Draw Data Lifecycle

- Draw data are created when a scene is activated or when a mesh instance is added at runtime.
- Draw data persist for the lifetime of its corresponding mesh instance and are destroyed when the mesh instance is destroyed, or the scene is unloaded.
- This ensures GPU resources are always in sync with the active scene.
- On scene switch, DrawDataManager clears and repopulates all draw data and mappings.

---

### Node-to-Draw Index Mapping

DrawDataManager maintains a mapping from scene nodes to their associated draw indices (one per mesh section). This enables efficient, targeted updates:

- When a node's transform changes, DrawDataManager is notified via the observer pattern (by the Scene or another subject). The notification includes the set of changed nodes, and DrawDataManager updates only the relevant draw data and culling entries. See [Observer Pattern](../core/observer_pattern.md) for details.
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

## Transform Synchronization & Draw Data Updates

When a node's transform changes (e.g., due to animation, movement, or scene graph updates), the DrawDataManager is notified (typically via the observer pattern from the Scene or another system). The DrawDataManager is responsible for updating all per-draw data that depends on transforms, including:

- `MeshDrawData.worldTransform`: Updated to reflect the new world transform for each mesh section attached to the node.
- `MeshDrawCullData.boundsToWorld`: Updated for each mesh section attached to the node, using the formula:
  ```cpp
  boundsToWorld = worldTransform * boundsToMesh;
  ```
  where `worldTransform` is the node's new world transform, and `boundsToMesh` is the static OBB in mesh local space from `MeshDrawBoundsData`.

**Example:**
```cpp
for (NodeHandle node : changedNodes) {
    glm::mat4 worldTransform = ...; // new world transform for node
    for (size_t drawIdx : drawDataManager.getDrawIndicesForNode(node)) {
        drawDataManager.meshDrawData[drawIdx].worldTransform = worldTransform;
        glm::mat4 boundsToMesh = drawDataManager.meshDrawBoundsData[drawIdx].boundsToMesh;
        drawDataManager.meshDrawCullData[drawIdx].boundsToWorld = worldTransform * boundsToMesh;
    }
}
```

- If a mesh section's bounds change (e.g., mesh deformation), update the corresponding `boundsToMesh` in `MeshDrawBoundsData` and resync transforms as needed.
- This design ensures all per-draw data is kept in sync and ready for culling and rendering after any transform or mesh update.

## 6. Example Usage

```cpp
// When switching scenes:
drawDataManager.clearAllData();
drawDataManager.populateFromScene(newScene); // Repopulates all draw data from the new scene

// When a node's transform or mesh/material assignment changes:
node->setLocalTransform(newTransform); // updates local transform
scene.propogateTransforms(); // propagates to children
scene.finalizeForRendering(); // Notifies observers of changed nodes and clears the changed set

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

## Dependency Injection and System Dependencies

The DrawDataManager depends on several systems for correct operation. All dependencies should be injected via constructor parameters, setter methods, or explicit registration APIs to enable modularity, testability, and decoupling.

### DrawDataManager Dependencies
- **RHI (RHIBase/IRHI)**: Injected via constructor or setter if DrawDataManager is responsible for GPU buffer/resource management. Allows for testable, decoupled GPU operations.
- **MeshSystem**: Injected via constructor or setter. Used to receive mesh instance changes.
- **MaterialSystem**: Injected via constructor or setter. Used to receive material changes.
- **Scene**: Registered as an observer to receive node transform changes.

#### Example (C++)
```cpp
DrawDataManager(RHIBase& rhi, MeshSystem* meshSys, MaterialSystem* materialSys);
void registerWithScene(Scene* scene) { scene->addObserver(this); }
```

- All dependencies, including the RHI, can be replaced with mocks or test doubles for unit testing.
- DrawDataManager does not own these systems; it only uses them via interfaces or pointers.

---

This document should be updated as the renderer and scene modules evolve.
