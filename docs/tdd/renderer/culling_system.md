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
- **Frustum planes are cached on the Camera. The CullingSystem does not store or cache frustum planes or projection matrix state.**
- **The CullingSystem obtains frustum planes and view matrix for culling by calling the camera's public API (e.g., `camera.getFrustumPlanes()` and `camera.getViewMatrix()`). The camera is responsible for ensuring these are up to date whenever its projection or transform changes. The culling system treats the camera as a stateless provider of frustum data, allowing culling from multiple views in the same frame without internal state management.**

---

## 3. API Overview

```cpp
// Called after scene graph transform update
void syncTransforms(const std::vector<NodeHandle>& changedNodes, const Scene& scene);

// Called when mesh draw cull data is added or removed
void addCullData(const glm::mat4& worldTransform, const Bounds& bounds);
void removeCullData(size_t drawIndex);

// Culling
std::vector<uint32_t> cull(const Camera& camera); // Returns indices of visible draws

// Query
const MeshDrawCullData& getMeshDrawCullData(uint32_t index) const;
```

- The `cull` method takes a `Camera&` and uses the camera's cached frustum planes and view matrix. The camera is responsible for maintaining and updating its cached frustum planes when its projection matrix changes. The CullingSystem queries these planes as needed and does not store any per-camera state.
- **The CullingSystem uses the camera's API to obtain the current frustum planes (e.g., `camera.getFrustumPlanes()`) and view matrix (e.g., `camera.getViewMatrix()`) for each culling operation. This ensures that culling always uses the latest camera state, and allows the same culling system to be used with multiple cameras or views in a single frame.**

---

## 4. Culling Algorithm

- For each mesh draw, transform its bounds to view space using the draw's world transform and the current camera's view matrix (obtained via `camera.getViewMatrix()`).
- Perform frustum-AABB testing in view space using the camera's cached frustum planes (obtained via `camera.getFrustumPlanes()`).
- Only draws passing the test are considered visible.
- The CullingSystem does not cache or store any camera or frustum state, so it can cull from multiple camera views in the same frame without additional state management.

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

// During culling and rendering:
auto visibleDraws = cullingSystem.cull(camera); // Returns indices of visible mesh draws
for (uint32_t drawIdx : visibleDraws) {
    // drawIdx is an index into both meshDrawCullData and MeshDrawData
    // Use drawIdx directly to access per-draw culling data or draw data for rendering
    // Example:
    const auto& drawData = drawDataManager.getMeshDrawData()[drawIdx];
    // ... issue draw call using drawData ...
}
```

- The `visibleDraws` vector contains indices into both the CullingSystem's `meshDrawCullData` array and the DrawDataManager's `MeshDrawData` array. This allows direct, efficient access to all per-draw data needed for rendering visible objects, without further indirection or lookup.
- External code should not need to access `meshDrawCullData` directly except for debugging or advanced queries; it is used internally by the culling system.

---

## 8. Notes

- CullingSystem is global/singleton-like, but all culling data is per scene.
- All culling data is duplicated for performance; no indirection to scene graph or draw data.
- Transform and bounds updates are explicit and must be synchronized after scene changes.
- The MeshDrawCullData array is always kept in sync and 1:1 with the MeshDrawData array for fast lookup and iteration.
- Mapping from (scene, node) to mesh draw indices is maintained externally for transform sync.

---

## 9. Suggested Tests

### Unit Tests

- **Frustum Culling Logic**
  - Given a set of mesh draws with known world transforms and bounds, verify that `cull(camera)` returns the correct set of visible indices for a variety of camera positions and orientations.
  - Test with objects fully inside, fully outside, and intersecting the frustum.
  - Test with degenerate bounds (zero size, negative extents) to ensure robust handling.

- **Transform Synchronization**
  - After calling `syncTransforms` with a set of changed nodes, verify that only the corresponding entries in `meshDrawCullData` are updated.
  - Confirm that repeated calls with the same unchanged nodes do not result in unnecessary updates.

- **Frustum Plane Usage**
  - Mock a camera with known frustum planes and verify that the culling system uses the camera's cached planes, not its own state.
  - Change the camera's projection matrix and ensure that the culling system reflects the updated frustum planes on the next cull.

- **Add/Remove Cull Data**
  - Add and remove cull data entries and verify that the internal array remains tightly packed and 1:1 with MeshDrawData.
  - Confirm that removed indices are not returned by `cull` and that subsequent additions are handled correctly.

- **Query API**
  - For a known index, verify that `getMeshDrawCullData(index)` returns the expected transform and bounds.

### Integration Tests

- **Scene Transform Propagation**
  - Modify node transforms in the scene, propagate with `updateWorldTransforms`, and synchronize with `syncTransforms`. Verify that culling results reflect the new transforms.
  - Attach/detach mesh draws to nodes and ensure culling results update accordingly.

- **Multi-Camera/Multiview Support**
  - Cull the same set of mesh draws from multiple cameras in a single frame. Verify that results are correct and independent for each camera.

- **Draw Data Synchronization**
  - After mesh draw data is added/removed in the DrawDataManager, ensure the culling system's data remains in sync and no stale indices are returned.

### Edge Cases

- **Empty Scene**
  - Call `cull` on an empty culling system and verify that the result is empty and no errors occur.

- **All Objects Outside Frustum**
  - Place all mesh draws outside the frustum and verify that `cull` returns an empty set.

- **All Objects Inside Frustum**
  - Place all mesh draws inside the frustum and verify that `cull` returns all indices.

- **Large Number of Draws**
  - Test with a large number of mesh draws (e.g., 10,000+) to ensure performance and correctness under load.

### Visual/Manual Validation

- **Debug Visualization**
  - Render the frustum and bounding volumes for a test scene. Visually confirm that only objects inside/intersecting the frustum are rendered.

- **Camera Movement**
  - Move the camera through the scene and verify that objects enter and exit the visible set as expected.

---

This document should be updated as the culling system evolves.
