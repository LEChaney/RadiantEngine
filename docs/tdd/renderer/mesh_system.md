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
- When a mesh instance is added to a scene that is currently active in the renderer, the MeshSystem will automatically create the corresponding draw data by calling `drawDataManager.createDrawDataForMeshInstance(meshInstance)`. This ensures that new mesh instances are immediately available for rendering. If the scene is not active, draw data will be created when the scene is later set active and the draw data is repopulated.
- The active scene is set on the renderer, which determines which scene is currently being rendered and which mesh instances will have draw data created immediately.

---

## 3. Data Structures

```cpp
struct MeshSection {
    uint32_t firstIndex;
    uint32_t indexCount;
    mat4 boundsToMesh;
    std::string name;
    // (Optional: user data, etc.)
};

struct Mesh {
    GPUBuffer vertexBuffer;
    GPUBuffer indexBuffer;
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
std::unordered_map<Scene*, std::vector<Mesh>> sceneMeshes;
std::unordered_map<Scene*, std::vector<MeshInstance>> sceneMeshInstances;
std::unordered_map<Scene*, std::unordered_map<NodeHandle, uint32_t>> nodeToMeshInstanceIndex;
```

---

## 3a. Mapping from (Scene, Node) to Mesh Draw Indices

*This mapping is now maintained by the DrawDataManager. See the DrawDataManager section in drawdata.md for details.*

---

## 4. API Overview

```cpp
// Mesh loading/unloading
Mesh* loadMesh(Scene* scene, const MeshSource& src); // Loads mesh for a scene (src contains raw vertex and index data)
void unloadAllMeshes(Scene* scene); // Unloads all meshes for a scene

// Node/section association and material assignment
MeshInstance& addMeshInstance(Scene* scene, NodeHandle node, Mesh* mesh, const MaterialSet& materials); // Assigns mesh and material set to node
void removeMeshInstance(Scene* scene, NodeHandle node);

// Querying
const std::vector<Mesh>& getMeshes(Scene* scene) const;
const std::vector<MeshInstance>& getMeshInstances(Scene* scene) const;
MeshInstance* getMeshInstanceForNode(Scene* scene, NodeHandle node) const;
const std::vector<MeshSection>& getSections(const Mesh* mesh) const;

// GPU resource management (internal, used during load and unload)
GPUBuffer createVertexBuffer(const Vertex* vertices, size_t count);
GPUBuffer createIndexBuffer(const uint32_t* indices, size_t count);
void destroyBuffer(GPUBuffer buffer);
```

---

## 5. Resource Management Flow

1. **Scene Load:**
    - For each mesh asset referenced in the scene, `loadMesh` is called.
    - The MeshSystem allocates GPU buffers via the Resource Allocator.
    - Meshes are registered in the per-scene mesh registry.
    - For each node that references a mesh, a `MeshInstance` is created and associated, along with its material set (one per mesh section).
    - Each mesh section is assigned bounds.
2. **During Runtime:**
    - MeshSystem maintains all mesh and instance data for the scene.
    - Provides mesh and section data to the DrawDataManager for draw data generation.
    - Materials for each draw are taken from the instance's material set.
    - When a mesh instance is added to the active scene, MeshSystem will automatically call `drawDataManager.createDrawDataForMeshInstance(meshInstance)` to create the necessary draw data for rendering.
3. **Node Removal:**
    - When a node is removed, the MeshSystem removes the corresponding `MeshInstance`.
    - The mesh resource remains alive until the scene is unloaded.
4. **Scene Unload:**
    - All meshes and mesh instances for the scene are destroyed.
    - All GPU buffers are deallocated via the Resource Allocator.

---

## 6. Integration with Other Systems

- **Resource Allocator:** Used for all GPU buffer allocations and deallocations.
- **DrawDataManager:** Consumes mesh and section data to generate draw data for rendering. **Relies on the Mesh System's (scene, node) → draw indices mapping for efficient transform synchronization.**
- **MaterialSystem:** Each mesh instance references a set of materials (one per section); actual material data is managed by the MaterialSystem.
- **Scene Graph:** MeshSystem does not own nodes; it only associates meshes to nodes via `NodeHandle`.

---

## 7. Example Usage

```cpp
// During scene loading:
Mesh* mesh = meshSystem.loadMesh(scene, meshSource); // meshSource contains vertex and index data
MaterialSet matSet = { matA, matB }; // One per mesh section
meshSystem.addMeshInstance(scene, node, mesh, matSet); // Assign mesh and material set to node in scene graph

// During node removal:
meshSystem.removeMeshInstance(scene, node);

// On scene unload:
meshSystem.unloadAllMeshes(scene);

// When dynamically adding a mesh instance to the active scene:
MeshInstance& meshInstance = meshSystem.addMeshInstance(activeScene, node, mesh, matSet); // Automatically creates draw data for the new mesh instance if the scene is active
// (Internally, this will call drawDataManager.createDrawDataForMeshInstance(meshInstance);)
```

---

## 8. Notes

- MeshSystem is global/singleton-like, but all mesh data is owned per scene.
- No mesh or buffer is ever shared between scenes.
- All GPU resource allocation is delegated to the Resource Allocator.
- MeshSystem does not perform reference counting; lifetime is managed by scene load/unload.
- MeshInstances enable multiple nodes in a scene to reference the same mesh, each with its own material set.
- Each mesh section can have a different material and bounds, and is the unit of culling and draw call generation.
- The Mesh System maintains the mapping from (scene, node) to mesh draw indices, which is used by the CullingSystem and DrawDataManager for efficient transform synchronization. This mapping is not stored in those systems.

---

This document should be updated as the mesh system evolves.
