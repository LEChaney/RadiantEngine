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
- Represents a single draw call group for a mesh section/material combination (instanced draw call).
- **Does not store per-instance data directly; instead, holds handles/indices into a global instance slot map.**
- Fields:
  - `uint32 indexCount`
  - `uint32 firstIndex`
  - `VkBuffer indexBuffer`
  - `VkDeviceAddress vertexBufferAddress`
  - `MaterialDrawData materialDrawData` // Inline struct, see above
  - `std::vector<InstanceHandle> instanceHandles` // Indices/handles into DrawDataManager's instance slot map
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
Owns and manages all per-draw arrays required for rendering and culling, using slot maps for all dynamic data:
  - `SlotMap<MeshDrawData> meshDrawData;` // Per mesh section/material group (draw call group)
  - `SlotMap<InstanceData> instanceSlotMap;` // Flat, global storage for all instance data (transforms, material overrides, etc.)
  - `SlotMap<MeshDrawCullData> meshDrawCullData;` // Per mesh section, for culling
  - `SlotMap<MeshDrawBoundsData> meshDrawBoundsData;` // Per mesh section, for culling
  - `SlotMap<LightDrawData> lightDrawData;`
  - `std::unordered_map<NodeHandle, std::vector<InstanceHandle>> nodeToInstanceHandles;` // Maps scene nodes to their instance handles in the slot map
  - `std::unordered_map<InstanceHandle, MeshDrawDataHandle> instanceToDrawDataHandle;` // Reverse lookup: maps each instance to its parent MeshDrawData
Methods:
  - `void clearAllData();` // Clears all draw/culling/instance data and mappings.
  - `void populateFromScene(Scene* scene);` // Populates all draw/culling/instance data and mappings from the given scene.
  - `const SlotMap<MeshDrawData>& getMeshDrawData() const`
  - `const SlotMap<InstanceData>& getInstanceSlotMap() const`
  - `const SlotMap<MeshDrawCullData>& getMeshDrawCullData() const`
  - `const SlotMap<MeshDrawBoundsData>& getMeshDrawBoundsData() const`
  - `void createDrawDataForMeshInstance(const MeshInstance& meshInstance);` // Allocates instance data in slot map, updates MeshDrawData
  - `void onTransformsFinalized(const std::vector<NodeHandle>& changedNodes);` // Observer pattern callback: updates instance data for changed nodes

---

## 5. Data Flow & Integration

- DrawDataManager tracks all instance data in a global slot map, and each MeshDrawData stores handles/indices into this map for its instances.
- **MeshDrawData Creation:**
    - When a mesh instance is added, DrawDataManager determines the draw group (mesh section + material combination) it belongs to.
    - If a MeshDrawData for that group does not exist, a new MeshDrawData is created and registered.
    - The new instance's handle is added to the `instanceHandles` of the appropriate MeshDrawData.
- **MeshDrawData Removal:**
    - When a mesh instance is removed, its handle is removed from the corresponding MeshDrawData's `instanceHandles`.
    - If a MeshDrawData's `instanceHandles` becomes empty (no instances left for that group), the MeshDrawData is removed and destroyed.
- **MeshDrawData Lifetime:**
    - MeshDrawData objects are created on-demand as new unique (mesh section, material) groups appear, and destroyed when no instances remain for that group.
- When a node's transform or material changes, DrawDataManager uses the node-to-instance-handle mapping to update only the relevant InstanceData entries.
- Before rendering, `syncGpuBuffers()` linearly copies all valid instance slot map entries to a GPU buffer for instanced rendering.
- The renderer issues instanced draw calls using MeshDrawData and the global instance buffer.
- When switching scenes, DrawDataManager clears all data and repopulates from the new scene.
- **Observer Pattern:** DrawDataManager acts as an observer to MeshSystem and MaterialSystem, updating instance data and draw groupings in response to notifications about mesh instance or material changes. See [Observer Pattern](../core/observer_pattern.md) for details.
- **Testability:** Observer registration is explicit and can be performed with either real or mock systems, supporting robust unit and integration testing.

---

## 5a. Draw Data Lifecycle

- Draw data are created when a scene is activated or when a mesh instance is added at runtime.
- Draw data persist for the lifetime of its corresponding mesh instance and are destroyed when the mesh instance is destroyed, or the scene is unloaded.
- This ensures GPU resources are always in sync with the active scene.
- On scene switch, DrawDataManager clears and repopulates all draw data and mappings.

---

### Node-to-Instance Mapping

DrawDataManager maintains a mapping from scene nodes to their associated instance handles in the slot map. This enables efficient, targeted updates:

- When a node's transform changes, DrawDataManager is notified via the observer pattern (by the Scene or another subject). The notification includes the set of changed nodes, and DrawDataManager updates only the relevant InstanceData entries in the slot map. See [Observer Pattern](../core/observer_pattern.md) for details.
- The mapping is updated whenever mesh instances are added, removed, or when the scene is loaded/unloaded.
- Only DrawDataManager owns and maintains this mapping; other systems query it as needed.

**Example:**
```cpp
for (NodeHandle node : changedNodes) {
    for (InstanceHandle inst : drawDataManager.getInstanceHandlesForNode(node)) {
        drawDataManager.instanceSlotMap[inst].worldTransform = newWorldTransform;
        // To update culling data, use the reverse lookup:
        MeshDrawDataHandle drawHandle = drawDataManager.instanceToDrawDataHandle[inst];
        glm::mat4 boundsToMesh = drawDataManager.meshDrawBoundsData[drawHandle].boundsToMesh;
        drawDataManager.meshDrawCullData[inst].boundsToWorld = newWorldTransform * boundsToMesh;
    }
}
```
This design enables flat, cache-friendly, and parallel updates to per-instance data after scene changes. Using slot maps for all dynamic arrays ensures that handles remain valid and all associations are robust, even as data is added or removed at runtime.

**TODO**
Note: We are using the same handles to look up multiple slotmaps here. While these slotmaps should be in sync, it might be a good idea to figure out some better structure for slotmap setups like this.

---

## Transform Synchronization & Draw Data Updates


When a node's transform changes (e.g., due to animation, movement, or scene graph updates), the DrawDataManager is notified (typically via the observer pattern from the Scene or another system). The DrawDataManager is responsible for updating all per-instance data that depends on transforms, including:

- `InstanceData.worldTransform`: Updated to reflect the new world transform for each instance attached to the node.
- `MeshDrawCullData.boundsToWorld`: Updated for each instance, using the formula:
  ```cpp
  boundsToWorld = worldTransform * boundsToMesh;
  ```
  where `worldTransform` is the instance's new world transform, and `boundsToMesh` is the static OBB in mesh local space from `MeshDrawBoundsData`.

**Example:**
```cpp
for (NodeHandle node : changedNodes) {
    glm::mat4 worldTransform = ...; // new world transform for node
    for (InstanceHandle inst : drawDataManager.getInstanceHandlesForNode(node)) {
        drawDataManager.instanceSlotMap[inst].worldTransform = worldTransform;
        // Optionally update culling data:
        glm::mat4 boundsToMesh = ...; // from mesh section
        drawDataManager.meshDrawCullData[inst].boundsToWorld = worldTransform * boundsToMesh;
    }
}
```

- If a mesh section's bounds change (e.g., mesh deformation), update the corresponding `boundsToMesh` in `MeshDrawBoundsData` and resync transforms as needed.
- This design ensures all per-instance data is kept in sync and ready for culling and rendering after any transform or mesh update.

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

// Rendering (per mesh section/material group):
for (const auto& drawData : drawDataManager.getMeshDrawData()) {
    // Upload instance data for drawData.instanceHandles to GPU buffer
    // vkCmdBindPipeline(..., drawData.pipeline, ...);
    // vkCmdBindDescriptorSets(..., drawData.pipelineLayout, drawData.materialDescriptorSet, ...);
    // vkCmdDrawIndexed(..., drawData.indexCount, drawData.instanceHandles.size(), drawData.firstIndex, ...);
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

---


## 9. Notes on Slot Map-Based Data and Reverse Lookup

- Using slot maps for all dynamic data (draw groups, instances, culling, bounds, lights) ensures that handles remain valid and all associations are robust, even as data is added or removed at runtime.
- MeshDrawData only stores handles/indices into the slot map, not the instance data itself.
- Adding/removing instances or draw groups is O(1) and does not invalidate unrelated handles.
- Node-to-instance-handle mapping enables fast per-node updates.
- The reverse lookup map (`instanceToDrawDataHandle`) allows O(1) access from an instance to its parent draw group, enabling efficient lookup of mesh section indices and bounds data for culling and transform updates.
- This approach is suitable for both rasterization (instanced draw calls) and ray tracing (TLAS instance data, SBT offset per instance).
- The slot maps can be compacted or defragmented as needed for optimal memory usage.

This document should be updated as the renderer and scene modules evolve.
