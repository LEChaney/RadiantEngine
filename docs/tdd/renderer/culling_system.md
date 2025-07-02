# Culling System Design Document

## Purpose

The CullingSystem is responsible for efficient visibility determination of mesh draws before rendering. It maintains its own tightly-packed data for culling, including per-draw world transforms and bounds, to enable fast, cache-friendly iteration and SIMD-friendly culling algorithms. Culling is performed in view space using the current view matrix and frustum planes. This system is decoupled from the draw data and scene graph for maximum performance.

---

## Responsibilities

- Store all data required for culling (world transform, bounds) per mesh draw.
- Provide fast, linear iteration for culling algorithms (frustum, occlusion, etc.) in view space.
- Synchronize culling data with the scene graph after transform changes.
- Expose APIs for updating transforms and bounds after scene or node changes.
- Report visible mesh draws for rendering.
- Provide APIs for clearing all culling data and repopulating from a new scene.

---

## Data Structures

```cpp
struct MeshDrawCullData {
    glm::mat4 boundsToWorld; // Cached OBB in world space (for culling)
};

struct MeshDrawBoundsData {
    glm::mat4 boundsToMesh; // OBB in mesh local space (static, only needed for resync)
};

std::vector<MeshDrawCullData> meshDrawCullData; // 1:1 with MeshDrawData
std::vector<MeshDrawBoundsData> meshDrawBoundsData; // 1:1 with MeshDrawData
```

- Each mesh draw now has two associated matrices:
  - `boundsToWorld`: The OBB in world space, updated whenever the owning node's world transform changes. Used for culling.
  - `boundsToMesh`: The OBB in mesh local space, typically set at creation and only updated if the mesh section's bounds change.
- Both arrays are kept in sync and indexed 1:1 with MeshDrawData.

---

## API Overview

The CullingSystem exposes the following key APIs:

```cpp
// Scene repopulation (called by Renderer on scene switch)
void clearAllData(); // Clears all culling and bounds data.
void populateFromScene(Scene* scene); // Populates all culling and bounds data from the given scene;

// Observer pattern callback for transform changes
void onNodeTransformsChanged(const std::vector<NodeHandle>& changedNodes) override; // Updates culling data for changed nodes

// Called when mesh draw cull data is added or removed
void addCullData(const glm::mat4& boundsToMesh, const glm::mat4& meshToWorld);
void removeCullData(size_t drawIndex);

// Culling
std::vector<uint32_t> cull(const Camera& camera); // Returns indices of visible draws

// Query
const MeshDrawCullData& getMeshDrawCullData(uint32_t index) const;
const MeshDrawBoundsData& getMeshDrawBoundsData(uint32_t index) const;
```

---

## Culling Algorithm
### Frustum Culling Algorithm Overview

Frustum culling in view space involves determining whether each object's bounding volume (typically an oriented bounding box, or OBB, represented as a mat4) intersects the camera's view frustum. Performing culling in view space—where the camera is at the origin and aligned with the axes—simplifies the intersection tests and improves numerical stability.

### Algorithm Steps

**1. Transform OBB to View Space:**
For each mesh draw, compute a conservative view-space AABB from its OBB using the following pseudo code (see geometry.md for OBB definition):

```cpp
// Inputs:
// - boundsToWorld: 4x4 OBB matrix (see geometry.md)
// - view: 4x4 view matrix

// 1. Combine transforms to get bounds-to-view matrix
boundsToView = view * boundsToWorld;

// 2. Transform the AABB center to view space
center_vs = boundsToView[3].xyz; // translation component

// 3. Compute conservative view-space extents
//    - Take the absolute value of the upper 3x3 part of the matrix (rotation, scale, shear)
//    - Multiply each column by the corresponding bounds-space extent
absRotScale = Abs3x3(boundsToView); // Each element is abs(matrix[i][j])
extents_vs = absRotScale * vec3(1, 1, 1);

// 4. The view-space AABB is defined by center_vs and extents_vs
```

**Explanation:**
- The OBB is represented as a 4x4 matrix as described in geometry.md. The first three columns encode the half-axes (scaled by extents), and the fourth column is the center.
- `Abs3x3` means taking the absolute value of each element in the 3x3 rotation/scale/shear part of the matrix.
- Multiplying this matrix by the bounds-space extents gives the maximum reach of the transformed box along each view-space axis, conservatively enclosing the oriented box.

For more details, see [Converting OBB to AABB in Target Space](https://madmann91.github.io/2024/02/10/converting-oriented-bounding-boxes-to-axis-aligned-ones.html) and the geometry.md documentation.

**2. Frustum Plane Extraction**
Obtain the camera's six frustum planes in view space (left, right, top, bottom, near, far), typically via `camera.getFrustumPlanes()`. Because these planes are defined in view space, their orientation and position are fixed relative to the camera axes, simplifying intersection tests and allowing them to be efficiently reused or cached on the camera.

**3. AABB-Frustum Plane Testing**
For each transformed AABB, test it against all frustum planes using the following method:

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
- [Geometry Primitives and Operations](../utils/geometry.md)

---

## Transform Synchronization

- When a node's transform changes, the CullingSystem is notified via the observer pattern (by the Scene or DrawDataManager). The notification includes the set of changed nodes, and CullingSystem updates only the relevant culling data. See [Observer Pattern](../core/observer_pattern.md) for details.
- For each affected mesh draw, the CullingSystem recomputes `boundsToWorld` as:
  ```cpp
  boundsToWorld = worldTransform * boundsToMesh;
  ```
  - `worldTransform` is read from the changed node.
  - `boundsToMesh` is read from the CullingSystem's own `meshDrawBoundsData` array.
- This avoids the need to query the MeshSystem during sync, and ensures all culling data is up to date and cache-friendly.

**Example Pseudocode:**
```cpp
for (NodeHandle node : changedNodes) {
    glm::mat4 worldTransform = node.worldTransform;
    const auto& drawIndices = drawDataManager.getDrawIndicesForNode(scene, node);
    for (size_t drawIdx : drawIndices) {
        glm::mat4 boundsToMesh = meshDrawBoundsData[drawIdx].boundsToMesh;
        meshDrawCullData[drawIdx].boundsToWorld = worldTransform * boundsToMesh;
    }
}
```

- If a mesh section's bounds change (e.g., mesh deformation), update the corresponding `boundsToMesh` in `meshDrawBoundsData` and resync transforms as needed.
- This design keeps the CullingSystem self-contained and efficient for both culling and transform updates.

---

## Integration with Scene and DrawData

- The CullingSystem does not store or access draw data or rendering state.
- It only tracks transforms and bounds for culling, with OBBs represented as mat4s.
- When the scene is switched, the Renderer calls `clearAllData()` and then `populateFromScene(newScene)` to repopulate all culling and bounds data from the new scene's mesh/instance data.
- The DrawDataManager and renderer receive the list of visible draw indices from the CullingSystem for rendering.
- Both CullingSystem and DrawDataManager store their own transforms for maximum iteration speed.
- There is a one-to-one mapping between the MeshDrawCullData and MeshDrawData arrays.

---

## Integration with Observer Pattern

- The CullingSystem may act as an observer to the Scene or DrawDataManager, receiving notifications when node transforms or draw data change. This enables efficient, decoupled synchronization of culling data. See [Observer Pattern](../core/observer_pattern.md) for details.
- **Testability:** Observer registration is explicit and can be performed with either real or mock systems, supporting robust unit and integration testing.

---

## Dependency Injection and System Dependencies

The CullingSystem depends on several systems for correct operation. All dependencies should be injected via constructor parameters, setter methods, or explicit registration APIs to enable modularity, testability, and decoupling.

### CullingSystem Dependencies
- **RHI (RHIBase/IRHI)**: Injected via constructor or setter if the CullingSystem performs GPU-side culling or manages GPU buffers. Allows for testable, decoupled GPU operations.
- **DrawDataManager**: Injected via constructor or setter. Used to map nodes to draw indices and synchronize culling data.
- **Scene**: Registered as an observer to receive node transform changes.

#### Example (C++)
```cpp
CullingSystem(RHIBase& rhi, DrawDataManager* drawDataMgr);
void registerWithScene(Scene* scene) { scene->addObserver(this); }
```

- All dependencies, including the RHI, can be replaced with mocks or test doubles for unit testing.
- CullingSystem does not own these systems; it only uses them via interfaces or pointers.

---

## Example Usage

```cpp
// When switching scenes:
cullingSystem.clearAllData();
cullingSystem.populateFromScene(newScene); // Repopulates all culling data from the new scene

// After scene graph transform update:
scene.propogateTransforms();
cullingSystem.onNodeTransformsChanged(changedNodes); // Observer pattern callback for transform changes

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
- The OBB for each draw is always represented as a mat4, as described in geometry.md.

## Suggested Tests

### Unit Tests

- **Frustum Culling Logic**
  - Given a set of mesh draws with known world OBB transforms (mat4) and bounds, verify that `cull(camera)` returns the correct set of visible indices for a variety of camera positions and orientations.
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
  - For a known index, verify that `getMeshDrawCullData(index)` returns the expected OBB transform (mat4).

### Integration Tests

- **Scene Transform Propagation**
  - Modify node transforms in the scene, call `scene.propogateTransforms()`, and notify the culling system using `cullingSystem.onNodeTransformsChanged(changedNodes)`. Verify that culling results reflect the updated transforms.
  - Attach or detach mesh draws to nodes, then notify the culling system of changes as appropriate. Ensure that culling results update to match the new scene structure.

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
