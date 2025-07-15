# Mesh System Design Document

## Purpose

The MeshSystem is responsible for managing mesh resources within each loaded scene, including their GPU buffers and associations to scene nodes. It provides APIs for mesh creation, destruction, and querying, and ensures all mesh data is scene-local with no sharing between scenes. The system supports multi-section meshes, per-instance material assignment, and integrates with the Resource Allocator for efficient GPU resource management.

---

## 1. Responsibilities

- **Manage mesh resources** (GPU buffers only) for all loaded scenes.
- **Support multi-section meshes:** Each mesh can have multiple sections, each with its own bounds.
- **Associate meshes to scene nodes** via `NodeHandle`.
- **Assign materials per mesh instance:** Each mesh instance provides a set of materials (one per mesh section) to use for rendering.
- **Load and unload meshes** on scene load/unload, ensuring robust cleanup.
- **Track all mesh allocations** per scene for efficient release.
- **Provide APIs** for mesh creation, destruction, node/section association, and querying.
- **Integrate with the Resource Allocator** for buffer allocation and deallocation.
- **Expose mesh and section data** for draw data generation and renderer consumption.
- **Handle material assignment** per mesh instance, in cooperation with the MaterialSystem.

---

## 2. Ownership and Lifetime

- All meshes are owned by a specific scene; there is no sharing or reference counting between scenes.
- The MeshSystem maintains an explicit mapping from `Scene*` (or scene ID) to all mesh resources owned by that scene.
- Meshes are loaded/created only when a scene is loaded, and destroyed only when the scene is unloaded.
- When a node is removed, the MeshSystem detaches any mesh/section association, but the mesh resource remains alive until the scene is unloaded.
- When a mesh instance is added to a scene that is currently active in the renderer, the MeshSystem will automatically create the corresponding draw data by calling `draw_data_manager.create_draw_data_for_mesh_instance(mesh_instance)`. This ensures that new mesh instances are immediately available for rendering. If the scene is not active, draw data will be created when the scene is later set active and the draw data is repopulated.
- The active scene is set on the renderer, which determines which scene is currently being rendered and which mesh instances will have draw data created immediately.

---

## 3. Data Structures

```cpp
struct MeshSection {
    uint32_t first_index;
    uint32_t index_count;
    mat4 bounds_to_mesh;
    std::string name;
    // (Optional: user data, etc.)
};

struct Mesh {
    GPUBuffer vertex_buffer;
    GPUBuffer index_buffer;
    std::vector<MeshSection> sections; // Each section has its own bounds
    std::string name;
    // ...
};

// A MaterialSet defines the materials to use for each mesh section in a given instance
using MaterialSet = std::vector<MaterialHandle>;

struct MeshInstance {
    NodeHandle node;
    Mesh* mesh;
    MaterialSet materials; // One material per mesh section
    // (Optional: per-instance data)
};

// Per-scene mesh registry
std::unordered_map<Scene*, std::vector<Mesh>> scene_meshes;
std::unordered_map<Scene*, std::vector<MeshInstance>> scene_mesh_instances;
std::unordered_map<Scene*, std::unordered_map<NodeHandle, uint32_t>> node_to_mesh_instance_index;
```

---

## 3a. Mapping from (Scene, Node) to Mesh Draw Indices

*This mapping is now maintained by the DrawDataManager. See the DrawDataManager section in drawdata.md for details.*

---

## 4. API Overview

```cpp
// Mesh loading/unloading
Mesh* load_mesh(Scene* scene, const MeshSource& src); // Loads mesh for a scene (src contains raw vertex and index data)
void unload_all_meshes(Scene* scene); // Unloads all meshes for a scene

// Node/section association and material assignment
MeshInstance& add_mesh_instance(Scene* scene, NodeHandle node, Mesh* mesh, const MaterialSet& materials); // Assigns mesh and material set to node
void remove_mesh_instance(Scene* scene, NodeHandle node);

// Querying
const std::vector<Mesh>& get_meshes(Scene* scene) const;
const std::vector<MeshInstance>& get_mesh_instances(Scene* scene) const;
MeshInstance* get_mesh_instance_for_node(Scene* scene, NodeHandle node) const;
const std::vector<MeshSection>& get_sections(const Mesh* mesh) const;

// GPU resource management (internal, used during load and unload)
GPUBuffer create_vertex_buffer(const Vertex* vertices, size_t count);
GPUBuffer create_index_buffer(const uint32_t* indices, size_t count);
void destroy_buffer(GPUBuffer buffer);
```

---

## 5. Resource Management Flow

1. **Scene Load:**
    - For each mesh asset referenced in the scene, `load_mesh` is called.
    - The MeshSystem allocates GPU buffers via the Resource Allocator.
    - Meshes are registered in the per-scene mesh registry.
    - For each node that references a mesh, a `MeshInstance` is created and associated, along with its material set (one per mesh section).
    - Each mesh section is assigned bounds.
2. **During Runtime:**
    - MeshSystem maintains all mesh and instance data for the scene.
    - Provides mesh and section data to the DrawDataManager for draw data generation.
    - Materials for each draw are taken from the instance's material set.
    - When a mesh instance is added to the active scene, MeshSystem will automatically call `draw_data_manager.create_draw_data_for_mesh_instance(mesh_instance)` to create the necessary instance data and update draw groupings for rendering. DrawDataManager uses a slot map to store all instance data flatly, and MeshDrawData only stores handles/indices into this slot map for instancing.
3. **Node Removal:**
    - When a node is removed, the MeshSystem removes the corresponding `MeshInstance`.
    - The mesh resource remains alive until the scene is unloaded. DrawDataManager erases the instance data from its slot map and updates draw groupings accordingly.
4. **Scene Unload:**
    - All meshes and mesh instances for the scene are destroyed.
    - All GPU buffers are deallocated via the Resource Allocator.
    - DrawDataManager clears all instance data and mappings from its slot map.

---

## 6. Integration with Other Systems

- **Resource Allocator:** Used for all GPU buffer allocations and deallocations.
- **DrawDataManager:** Consumes mesh and section data to generate draw data for rendering. **Relies on the Mesh System's (scene, node) → draw indices mapping for efficient transform synchronization.**
 **DrawDataManager:** Consumes mesh and section data to generate draw data for rendering. **DrawDataManager now maintains a mapping from (scene, node) to instance handles in its slot map for efficient transform synchronization and per-instance updates.**
- **MaterialSystem:** Each mesh instance references a set of materials (one per section); actual material data is managed by the MaterialSystem.
- **Scene Graph:** MeshSystem does not own nodes; it only associates meshes to nodes via `NodeHandle`.
- **Observer Pattern:** MeshSystem acts as a subject and notifies observers (e.g., DrawDataManager) when mesh instance assignments or per-instance material sets change. See [Observer Pattern](../core/observer_pattern.md) for details on decoupled update notification.
- **Testability:** Observer registration is explicit and can be performed with either real or mock systems, supporting robust unit and integration testing.

---

## 7. Example Usage

```cpp
// During scene loading:
Mesh* mesh = mesh_system.load_mesh(scene, mesh_source); // mesh_source contains vertex and index data
MaterialSet mat_set = { mat_a, mat_b }; // One per mesh section
mesh_system.add_mesh_instance(scene, node, mesh, mat_set); // Assign mesh and material set to node in scene graph

// During node removal:
mesh_system.remove_mesh_instance(scene, node);

// On scene unload:
mesh_system.unload_all_meshes(scene);

// When dynamically adding a mesh instance to the active scene:
MeshInstance& mesh_instance = mesh_system.add_mesh_instance(active_scene, node, mesh, mat_set); // Automatically creates draw data for the new mesh instance if the scene is active
// (Internally, this will call draw_data_manager.create_draw_data_for_mesh_instance(mesh_instance);)
```

---

## 8. Notes

- MeshSystem is global/singleton-like, but all mesh data is owned per scene.
- No mesh or buffer is ever shared between scenes.
- All GPU resource allocation is delegated to the Resource Allocator.
- MeshSystem does not perform reference counting; lifetime is managed by scene load/unload.
    - MeshInstances enable multiple nodes in a scene to reference the same mesh, each with its own material set.
    - Each mesh section can have a different material and bounds, and is the unit of culling and draw call generation.
    - DrawDataManager maintains a global slot map for all instance data, and MeshDrawData only stores handles/indices into this slot map for instancing. Node-to-instance-handle mapping enables efficient updates and synchronization.

---

---

## Notes on Automatic Instancing and Slot Map Integration

- DrawDataManager uses a slot map to store all instance data (transforms, per-instance material overrides, etc.) flatly and globally.
- When a mesh instance is added, its instance data is allocated in the slot map, and the handle is added to the appropriate MeshDrawData's list for instanced rendering.
- When a mesh instance is removed, its slot is erased and the handle is removed from the MeshDrawData's list.
- Node-to-instance-handle mapping enables fast per-node updates (e.g., transform changes, material overrides).
- This approach supports both rasterization (instanced draw calls) and ray tracing (TLAS instance data, SBT offset per instance).
- See `drawdata.md` for details on the slot map architecture and per-instance data management.

This document should be updated as the mesh system evolves.
