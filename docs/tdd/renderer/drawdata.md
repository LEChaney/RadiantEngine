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
  - `VkPipelineLayout pipeline_layout`
  - `VkDescriptorSet descriptor_set`

### MeshDrawData
- Represents a single draw call group for a mesh section/material combination (instanced draw call).
- **Does not store per-instance data directly; instead, holds handles/indices into a global instance slot map.**
- Fields:
  - `uint32 index_count`
  - `uint32 first_index`
  - `VkBuffer index_buffer`
  - `VkDeviceAddress vertex_buffer_address`
  - `MaterialDrawData material_draw_data` // Inline struct, see above
  - `std::vector<InstanceHandle> instance_handles` // Indices/handles into DrawDataManager's instance slot map
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
  - `glm::mat4 bounds_to_world` // OBB transform in world space (updated when node/world transform changes)

### MeshDrawBoundsData
- Stores the OBB for a mesh section in mesh local space (static, only needed for resync or mesh deformation).
- Fields:
  - `glm::mat4 bounds_to_mesh` // OBB transform in mesh local space

### DrawDataManager
Owns and manages all per-draw arrays required for rendering and culling, using slot maps for all dynamic data:
  - `SlotMap<MeshDrawData> mesh_draw_data;` // Per mesh section/material group (draw call group)
  - `SlotMap<InstanceData> instance_slot_map;` // Flat, global storage for all instance data (transforms, material overrides, etc.)
  - `SlotMap<MeshDrawCullData> mesh_draw_cull_data;` // Per mesh section, for culling
  - `SlotMap<MeshDrawBoundsData> mesh_draw_bounds_data;` // Per mesh section, for culling
  - `SlotMap<LightDrawData> light_draw_data;`
  - `std::unordered_map<NodeHandle, std::vector<InstanceHandle>> node_to_instance_handles;` // Maps scene nodes to their instance handles in the slot map
  - `std::unordered_map<InstanceHandle, MeshDrawDataHandle> instance_to_draw_data_handle;` // Reverse lookup: maps each instance to its parent MeshDrawData
Methods:
  - `void clear_all_data();` // Clears all draw/culling/instance data and mappings.
  - `void populate_from_scene(Scene* scene);` // Populates all draw/culling/instance data and mappings from the given scene.
  - `const SlotMap<MeshDrawData>& get_mesh_draw_data() const`
  - `const SlotMap<InstanceData>& get_instance_slot_map() const`
  - `const SlotMap<MeshDrawCullData>& get_mesh_draw_cull_data() const`
  - `const SlotMap<MeshDrawBoundsData>& get_mesh_draw_bounds_data() const`
  - `void create_draw_data_for_mesh_instance(const MeshInstance& mesh_instance);` // Allocates instance data in slot map, updates MeshDrawData
  - `void on_transforms_finalized(const std::vector<NodeHandle>& changed_nodes);` // Observer pattern callback: updates instance data for changed nodes

---

## 5. Data Flow & Integration

- DrawDataManager tracks all instance data in a global slot map, and each MeshDrawData stores handles/indices into this map for its instances.
- **MeshDrawData Creation:**
    - When a mesh instance is added, DrawDataManager determines the draw group (mesh section + material combination) it belongs to.
    - If a MeshDrawData for that group does not exist, a new MeshDrawData is created and registered.
    - The new instance's handle is added to the `instance_handles` of the appropriate MeshDrawData.
- **MeshDrawData Removal:**
    - When a mesh instance is removed, its handle is removed from the corresponding MeshDrawData's `instance_handles`.
    - If a MeshDrawData's `instance_handles` becomes empty (no instances left for that group), the MeshDrawData is removed and destroyed.
- **MeshDrawData Lifetime:**
    - MeshDrawData objects are created on-demand as new unique (mesh section, material) groups appear, and destroyed when no instances remain for that group.
- When a node's transform or material changes, DrawDataManager uses the node-to-instance-handle mapping to update only the relevant InstanceData entries.
- Before rendering, `sync_gpu_buffers()` linearly copies all valid instance slot map entries to a GPU buffer for instanced rendering.
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
for (NodeHandle node : changed_nodes) {
    for (InstanceHandle inst : draw_data_manager.get_instance_handles_for_node(node)) {
        draw_data_manager.instance_slot_map[inst].world_transform = new_world_transform;
        // To update culling data, use the reverse lookup:
        MeshDrawDataHandle draw_handle = draw_data_manager.instance_to_draw_data_handle[inst];
        glm::mat4 bounds_to_mesh = draw_data_manager.mesh_draw_bounds_data[draw_handle].bounds_to_mesh;
        draw_data_manager.mesh_draw_cull_data[inst].bounds_to_world = new_world_transform * bounds_to_mesh;
    }
}
```
This design enables flat, cache-friendly, and parallel updates to per-instance data after scene changes. Using slot maps for all dynamic arrays ensures that handles remain valid and all associations are robust, even as data is added or removed at runtime.

**TODO**
Note: We are using the same handles to look up multiple slotmaps here. While these slotmaps should be in sync, it might be a good idea to figure out some better structure for slotmap setups like this.

---

## Transform Synchronization & Draw Data Updates


When a node's transform changes (e.g., due to animation, movement, or scene graph updates), the DrawDataManager is notified (typically via the observer pattern from the Scene or another system). The DrawDataManager is responsible for updating all per-instance data that depends on transforms, including:

- `InstanceData.world_transform`: Updated to reflect the new world transform for each instance attached to the node.
- `MeshDrawCullData.bounds_to_world`: Updated for each instance, using the formula:
  ```cpp
  bounds_to_world = world_transform * bounds_to_mesh;
  ```
  where `world_transform` is the instance's new world transform, and `bounds_to_mesh` is the static OBB in mesh local space from `MeshDrawBoundsData`.

**Example:**
```cpp
for (NodeHandle node : changed_nodes) {
    glm::mat4 world_transform = ...; // new world transform for node
    for (InstanceHandle inst : draw_data_manager.get_instance_handles_for_node(node)) {
        draw_data_manager.instance_slot_map[inst].world_transform = world_transform;
        // Optionally update culling data:
        glm::mat4 bounds_to_mesh = ...; // from mesh section
        draw_data_manager.mesh_draw_cull_data[inst].bounds_to_world = world_transform * bounds_to_mesh;
    }
}
```

- If a mesh section's bounds change (e.g., mesh deformation), update the corresponding `bounds_to_mesh` in `MeshDrawBoundsData` and resync transforms as needed.
- This design ensures all per-instance data is kept in sync and ready for culling and rendering after any transform or mesh update.

## 6. Example Usage

```cpp
// When switching scenes:
draw_data_manager.clear_all_data();
draw_data_manager.populate_from_scene(new_scene); // Repopulates all draw data from the new scene

// When a node's transform or mesh/material assignment changes:
node->set_local_transform(new_transform); // updates local transform
scene.propogate_transforms(); // propagates to children
scene.finalize_for_rendering(); // Notifies observers of changed nodes and clears the changed set

// When dynamically adding a mesh instance to the active scene:
MeshInstance& mesh_instance = mesh_system.add_mesh_instance(active_scene, node, mesh, mat_set); // Automatically creates draw data for the new mesh instance if the scene is active
// (Internally, this will call draw_data_manager.create_draw_data_for_mesh_instance(mesh_instance);)

// At the start of the frame (before rendering):
draw_data_manager.sync_gpu_buffers(); // updates all dirty draw data GPU buffers, clears dirty flags

// Rendering (per mesh section/material group):
for (const auto& draw_data : draw_data_manager.get_mesh_draw_data()) {
    // Upload instance data for draw_data.instance_handles to GPU buffer
    // vkCmdBindPipeline(..., draw_data.pipeline, ...);
    // vkCmdBindDescriptorSets(..., draw_data.pipeline_layout, draw_data.material_descriptor_set, ...);
    // vkCmdDrawIndexed(..., draw_data.index_count, draw_data.instance_handles.size(), draw_data.first_index, ...);
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
DrawDataManager(RHIBase& rhi, MeshSystem* mesh_sys, MaterialSystem* material_sys);
void register_with_scene(Scene* scene) { scene->add_observer(this); }
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
- The reverse lookup map (`instance_to_draw_data_handle`) allows O(1) access from an instance to its parent draw group, enabling efficient lookup of mesh section indices and bounds data for culling and transform updates.
- This approach is suitable for both rasterization (instanced draw calls) and ray tracing (TLAS instance data, SBT offset per instance).
- The slot maps can be compacted or defragmented as needed for optimal memory usage.

This document should be updated as the renderer and scene modules evolve.
