# Mesh System Design Document

## Purpose

The MeshSystem manages all mesh resources for loaded scenes, handles their association to scene nodes, and provides APIs for mesh loading, unloading, node association, and querying. It ensures that all mesh data (GPU only) is owned per scene, with no cross-scene sharing, and integrates with the Resource Allocator for GPU resource management. Meshes can have multiple sections, each with its own material assignment, bounds, and buffer offsets.

---

## 1. Responsibilities

- **Manage mesh resources** (GPU buffers only) for all loaded scenes.
- **Support multi-section meshes:** Each mesh can have multiple sections, each with its own material and bounds.
- **Associate meshes and mesh sections to scene nodes** via `NodeHandle`.
- **Load and unload meshes** on scene load/unload, ensuring robust cleanup.
- **Track all mesh allocations** per scene for efficient release.
- **Provide APIs** for mesh creation, destruction, node/section association, and querying.
- **Integrate with the Resource Allocator** for buffer allocation and deallocation.
- **Expose mesh and section data** for draw data generation and renderer consumption.
- **Handle material assignment** per mesh section, in cooperation with the MaterialSystem.

---

## 2. Ownership and Lifetime

- All meshes are owned by a specific scene; there is no sharing or reference counting between scenes.
- The MeshSystem maintains an explicit mapping from `Scene*` (or scene ID) to all mesh resources owned by that scene.
- Meshes are loaded/created only when a scene is loaded, and destroyed only when the scene is unloaded.
- When a node is removed, the MeshSystem detaches any mesh/section association, but the mesh resource remains alive until the scene is unloaded.

---

## 3. Data Structures

```cpp
struct MeshSection {
    uint32_t firstIndex;
    uint32_t indexCount;
    uint32_t materialIndex; // Index into MaterialSystem/material registry
    Bounds bounds; // Per-section bounds for culling
    // (Optional: section name, user data, etc.)
};

struct Mesh {
    GPUBuffer vertexBuffer;
    GPUBuffer indexBuffer;
    std::vector<MeshSection> sections; // Each section has its own material and bounds
    std::string name;
    // ...
};

struct MeshInstance {
    NodeHandle node;
    Mesh* mesh;
    // (Optional: per-instance data)
};

// Per-scene mesh registry
std::unordered_map<Scene*, std::vector<Mesh>> sceneMeshes;
std::unordered_map<Scene*, std::vector<MeshInstance>> sceneMeshInstances;
```

---

## 3a. Mapping from (Scene, Node) to Mesh Draw Indices

### Purpose

The Mesh System is responsible for maintaining an explicit mapping from (scene, node) pairs to the indices of mesh draws (i.e., per-section draw calls) in the global draw data arrays. This mapping is essential for efficient synchronization of transforms and other per-draw data between the scene graph, CullingSystem, and DrawDataManager.

### Responsibilities
- For each mesh instance associated with a node in a scene, the Mesh System records which mesh draw indices (in the flat arrays managed by DrawDataManager and CullingSystem) correspond to that (scene, node) pair.
- When a node's transform changes, this mapping allows the CullingSystem and DrawDataManager to quickly update only the relevant draw data and culling data for that node, without searching or traversing the entire scene or draw arrays.
- The mapping is updated whenever mesh instances are added or removed, or when the scene is loaded/unloaded.

### Data Structure Example
```cpp
// For each scene, map from NodeHandle to a vector of draw indices (one per mesh section/draw)
std::unordered_map<Scene*, std::unordered_map<NodeHandle, std::vector<size_t>>> sceneNodeToDrawIndices;
```

### Usage in Transform Synchronization
- After the scene graph updates world transforms, it provides a list of changed nodes.
- The CullingSystem and DrawDataManager use the Mesh System's mapping to look up which draw indices correspond to each changed node.
- Only those draw data and culling data entries are updated, ensuring efficient, cache-friendly synchronization.

### Example
```cpp
// After scene update:
for (NodeHandle node : changedNodes) {
    const auto& drawIndices = meshSystem.getDrawIndicesForNode(scene, node);
    for (size_t drawIdx : drawIndices) {
        // Update transform in DrawDataManager and CullingSystem
    }
}
```

### Notes
- This mapping is maintained solely by the Mesh System; neither the CullingSystem nor DrawDataManager store or manage it.
- The mapping is always kept up to date with mesh instance and draw data creation/destruction.
- This design enables flat, cache-friendly, and parallel updates to per-draw data after scene changes.

---

## 4. API Overview

```cpp
// Mesh loading/unloading
Mesh* loadMesh(Scene* scene, const MeshSource& src); // Loads mesh for a scene
void unloadAllMeshes(Scene* scene); // Unloads all meshes for a scene

// Node/section association
void addMeshInstance(Scene* scene, NodeHandle node, Mesh* mesh);
void removeMeshInstance(Scene* scene, NodeHandle node);

// Material assignment (preliminary, see MaterialSystem)
void setSectionMaterial(Mesh* mesh, uint32_t sectionIndex, uint32_t materialIndex); // Assign material to section
uint32_t getSectionMaterial(const Mesh* mesh, uint32_t sectionIndex) const;

// Querying
const std::vector<Mesh>& getMeshes(Scene* scene) const;
const std::vector<MeshInstance>& getMeshInstances(Scene* scene) const;
Mesh* getMeshForNode(Scene* scene, NodeHandle node) const;
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
    - For each node that references a mesh, a `MeshInstance` is created and associated.
    - Each mesh section is assigned a material and bounds.
2. **During Runtime:**
    - MeshSystem maintains all mesh and instance data for the scene.
    - Provides mesh and section data to the DrawDataManager for draw data generation.
3. **Node Removal:**
    - When a node is removed, the MeshSystem removes the corresponding `MeshInstance`.
    - The mesh resource remains alive until the scene is unloaded.
4. **Scene Unload:**
    - All meshes and mesh instances for the scene are destroyed.
    - All GPU buffers are deallocated via the Resource Allocator.

---

## 6. Integration with Other Systems

- **Resource Allocator:** Used for all GPU buffer allocations and deallocations.
- **DrawDataManager:** Consumes mesh and section data to generate draw data for rendering. **Relies on the Mesh System's (scene, node) → draw indices mapping for efficient transform and data synchronization.**
- **MaterialSystem:** Each mesh section references a material by index; actual material data is managed by the MaterialSystem.
- **Scene Graph:** MeshSystem does not own nodes; it only associates meshes to nodes via `NodeHandle`.

---

## 7. Example Usage

```cpp
// During scene loading:
Mesh* mesh = meshSystem.loadMesh(scene, meshSource); // meshSource contains vertex and index data
meshSystem.setSectionMaterial(mesh, 0, materialIndex); // Assign material to section 0
meshSystem.addMeshInstance(scene, node, mesh); // Assign mesh to node in scene graph

// During node removal:
meshSystem.removeMeshInstance(scene, node);

// On scene unload:
meshSystem.unloadAllMeshes(scene);
```

---

## 8. Notes

- MeshSystem is global/singleton-like, but all mesh data is owned per scene.
- No mesh or buffer is ever shared between scenes.
- All GPU resource allocation is delegated to the Resource Allocator.
- MeshSystem does not perform reference counting; lifetime is managed by scene load/unload.
- MeshInstances enable multiple nodes in a scene to reference the same mesh.
- Each mesh section can have a different material and bounds, and is the unit of culling and draw call generation.
- The Mesh System maintains the mapping from (scene, node) to mesh draw indices, which is used by the CullingSystem and DrawDataManager for efficient transform synchronization. This mapping is not stored in those systems.

---

This document should be updated as the mesh system evolves.
