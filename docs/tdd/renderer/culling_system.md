# Culling System Design Document

## Purpose

The CullingSystem is responsible for efficient visibility determination of scene objects (meshes, mesh draws, etc.) before rendering. It maintains its own tightly-packed data for culling, including per-draw world transforms and bounds, to enable fast, cache-friendly iteration and SIMD-friendly culling algorithms. Culling is performed in view space using the current view matrix and frustum planes, matching the approach in the `is_in_frustum` implementation. This system is decoupled from the draw data and scene graph for maximum performance.

---

## 1. Responsibilities

- Store all data required for culling (world transform, bounds) per mesh draw.
- Provide fast, linear iteration for culling algorithms (frustum, occlusion, etc.) in view space.
- Synchronize culling data with the scene graph after transform changes.
- Expose APIs for updating transforms and bounds after scene or node changes.
- Report visible mesh draws for rendering.

---

## 2. Data Structures

```cpp
struct MeshDrawCullData {
    glm::mat4 worldTransform; // Per-draw world transform
    Bounds bounds;            // Per-draw bounds (in bounds space, e.g., AABB)
};

std::vector<MeshDrawCullData> meshDrawCullData; // 1:1 with MeshDrawData
```

- No mesh/section indices or NodeHandle are stored; the array is always kept in sync and 1:1 with MeshDrawData.
- A separate mapping from (scene, node) to mesh draw indices is maintained elsewhere for transform sync, but not in this struct.

---

## 3. API Overview

```cpp
// Called after scene graph transform update
void syncTransforms(const std::vector<NodeHandle>& changedNodes, const Scene& scene);

// Called when mesh draw cull data is added or removed
void addCullData(const glm::mat4& worldTransform, const Bounds& bounds);
void removeCullData(size_t drawIndex);

// Culling
std::vector<uint32_t> cull(const std::array<FrustumPlane, 6>& frustumPlanes, const glm::mat4& view) const; // Returns indices of visible draws

// Query
const MeshDrawCullData& getMeshDrawCullData(uint32_t index) const;
```

---

## 4. Culling Algorithm

- For each mesh draw, transform its bounds to view space using the draw's world transform and the current view matrix.
- Perform frustum-AABB testing in view space using the frustum planes, as in the `is_in_frustum` implementation.
- Only draws passing the test are considered visible.

---

## 5. Transform Synchronization

- When a node's transform changes (including parent propagation), the scene graph updates the world transform for all affected nodes.
- The CullingSystem is notified via `syncTransforms`, passing the list of changed nodes.
- The CullingSystem uses a mapping from (scene, node) to mesh draw indices to update the world transform for all affected draws.
- This ensures culling data is always up to date and decoupled from the scene graph.

---

## 6. Integration with Scene and DrawData

- The CullingSystem does not store or access draw data or rendering state.
- It only tracks transforms and bounds for culling.
- The DrawDataManager and renderer receive the list of visible draw indices from the CullingSystem for rendering.
- Both CullingSystem and DrawDataManager store their own transforms for maximum iteration speed.
- There is a one-to-one mapping between the MeshDrawCullData and MeshDrawData arrays.

---

## 7. Example Usage

```cpp
// After scene graph transform update:
scene.updateWorldTransforms();
cullingSystem.syncTransforms(changedNodes, scene);

// During culling:
auto visibleDraws = cullingSystem.cull(frustumPlanes, viewMatrix);
for (uint32_t idx : visibleDraws) {
    const auto& cullData = cullingSystem.getMeshDrawCullData(idx);
    // Pass to renderer/draw data
}
```

---

## 8. Notes

- CullingSystem is global/singleton-like, but all culling data is per scene.
- All culling data is duplicated for performance; no indirection to scene graph or draw data.
- Transform and bounds updates are explicit and must be synchronized after scene changes.
- The MeshDrawCullData array is always kept in sync and 1:1 with the MeshDrawData array for fast lookup and iteration.
- Mapping from (scene, node) to mesh draw indices is maintained externally for transform sync.

---

This document should be updated as the culling system evolves.
