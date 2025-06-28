# Culling System Design Document

## Purpose

The CullingSystem is responsible for efficient visibility determination of the visiblility of mesh draws before rendering. It maintains its own tightly-packed data for culling, including per-draw world transforms and bounds, to enable fast, cache-friendly iteration and SIMD-friendly culling algorithms. Culling is performed in view space using the current view matrix and frustum planes. This system is decoupled from the draw data and scene graph for maximum performance.

---

## Responsibilities

- Store all data required for culling (world transform, bounds) per mesh draw.
- Provide fast, linear iteration for culling algorithms (frustum, occlusion, etc.) in view space.
- Synchronize culling data with the scene graph after transform changes.
- Expose APIs for updating transforms and bounds after scene or node changes.
- Report visible mesh draws for rendering.

---

## Data Structures

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

## API Overview

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

- The CullingSystem uses the camera's API to obtain the current frustum planes (e.g., `camera.getFrustumPlanes()`) and view matrix (e.g., `camera.getViewMatrix()`) for each culling operation. This ensures that culling always uses the latest camera state, and allows the same culling system to be used with multiple cameras or views in a single frame.

---

## Culling Algorithm
### Frustum Culling Algorithm Overview

Frustum culling in view space involves determining whether each object's bounding volume (typically an axis-aligned bounding box, or AABB) intersects the camera's view frustum. Performing culling in view space—where the camera is at the origin and aligned with the axes—simplifies the intersection tests and improves numerical stability.

### Algorithm Steps

**1. Transform Bounds to View Space:**
For each mesh draw, compute a conservative view-space AABB from its bounds-space AABB using the following pseudo code:

```cpp
// Inputs:
// - boundsToWorld: 4x4 matrix transforming from bounds space to world space
// - view: 4x4 view matrix
// - aabb_center: center of the AABB in bounds space
// - aabb_extents: half-size (extent) of the AABB in bounds space

// 1. Combine transforms to get bounds-to-view matrix
boundsToView = view * boundsToWorld

// 2. Transform the AABB center to view space
center_vs = TransformPoint(boundsToView, aabb_center)

// 3. Compute conservative view-space extents
//    - Take the absolute value of the upper 3x3 part of the matrix (rotation, scale, shear)
//    - Multiply each column by the corresponding bounds-space extent
absRotScale = Abs3x3(boundsToView) // Each element is abs(matrix[i][j])
extents_vs = absRotScale * aabb_extents

// 4. The view-space AABB is defined by center_vs and extents_vs
```

**Explanation:**
- `Abs3x3` means taking the absolute value of each element in the 3x3 rotation/scale/shear part of the matrix.
- Multiplying this matrix by the bounds-space extents gives the maximum reach of the transformed box along each view-space axis, conservatively enclosing the oriented box.
- This method works for any AABB in any local space, and always produces a view-space AABB that fully contains the transformed box, regardless of rotation or scale.

For more details, see [Converting OBB to AABB in Target Space](https://madmann91.github.io/2024/02/10/converting-oriented-bounding-boxes-to-axis-aligned-ones.html).

**2. Frustum Plane Extraction**
Obtain the camera's six frustum planes in view space (left, right, top, bottom, near, far), typically via `camera.getFrustumPlanes()`. Because these planes are defined in view space, their orientation and position are fixed relative to the camera axes, simplifying intersection tests and allowing them to be efficiently reused or cached on the camera.

**3. AABB-Frustum Plane Testing**
For each transformed AABB, test it against all frustum planes using the following method:

To test an AABB (defined by its center and extents) against a plane:

```cpp
// AABB parameters in view space
vec3 center;   // Center of the AABB
vec3 extents;  // Half-size (extent) of the AABB
vec4 plane;    // Frustum plane (xyz = normal, w = distance)

// Compute signed distance from box center to plane
float d = dot(plane.xyz, center) + plane.w;

// Compute the maximum projected radius of the box along the plane normal
float r = dot(abs(plane.xyz), extents);

// Culling test
if (d + r < 0.0) {
  // The AABB is fully outside this frustum plane → cull
}
```

**Explanation:**  
The line `float r = dot(abs(plane.xyz), extents);` computes the maximum projected radius of the AABB along the plane normal.  
Using `abs(plane.xyz)` here effectively selects the AABB corner that yields the largest possible projection (`r`) onto the plane normal.  
For example, if the plane normal is `(-x, -y, +z)`, the corner `(-extents.x, -extents.y, +extents.z)` produces the largest `r` value.  
In general, the sign of each extent component is chosen to match the sign of the corresponding plane normal component, ensuring maximization.  
This is equivalent to taking the absolute value of the plane normal components and dotting with extents, which is both simpler and more efficient.

- If `d + r < 0.0`, the box is entirely outside the plane and can be culled.
- If `d - r > 0.0`, the box is entirely inside the plane (rarely used in practice).
- Otherwise, the box intersects the plane.

This approach is efficient and SIMD-friendly, and is commonly known as the "slab method" for AABB-plane intersection.

### References  
- [Real-Time Rendering, 4th Edition](https://www.realtimerendering.com/) – Section on frustum culling  
- [OGRE3D Frustum Culling](https://ogrecave.github.io/ogre/api/latest/classOgre_1_1Frustum.html)  
- [Converting OBB to AABB in Target Space](https://madmann91.github.io/2024/02/10/converting-oriented-bounding-boxes-to-axis-aligned-ones.html)

---

## Transform Synchronization

- When a node's transform changes the scene graph updates the world transform for all affected nodes.
- The CullingSystem is notified via `syncTransforms`, passing the list of changed nodes.
- The CullingSystem uses a mapping from (scene, node) to mesh draw indices to update the world transform for all affected draws.
- This ensures culling data is always up to date and decoupled from the scene graph.

---

## Integration with Scene and DrawData

- The CullingSystem does not store or access draw data or rendering state.
- It only tracks transforms and bounds for culling.
- The DrawDataManager and renderer receive the list of visible draw indices from the CullingSystem for rendering.
- Both CullingSystem and DrawDataManager store their own transforms for maximum iteration speed.
- There is a one-to-one mapping between the MeshDrawCullData and MeshDrawData arrays.

---

## Example Usage

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

## Suggested Tests

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
