# Culling System Algorithm Documentation

## Purpose

The CullingSystem provides high-performance algorithms for visibility determination of mesh draws before rendering. It is a stateless utility that operates on per-draw culling data arrays owned by the DrawDataManager. The primary function is frustum culling, which determines which mesh draws are visible to the camera.

---

## API

```cpp
// Returns indices of visible mesh draws given the camera and per-draw culling data.
std::vector<uint32_t> cull(const Camera& camera, const std::vector<MeshDrawCullData>& mesh_draw_cull_data);
```
- `camera`: The current camera, providing view/projection matrices and frustum planes.
- `mesh_draw_cull_data`: Array of per-draw OBBs in world space (one per mesh draw).

---

## Frustum Culling Algorithm

Frustum culling in view space determines whether each object's bounding volume (typically an oriented bounding box, or OBB, represented as a mat4) intersects the camera's view frustum. Performing culling in view space—where the camera is at the origin and aligned with the axes—simplifies intersection tests and improves numerical stability.

### Algorithm Steps

**1. Transform OBB to View Space:**
For each mesh draw, compute a conservative view-space AABB from its OBB using the following pseudo code (see geometry.md for OBB definition):

```cpp
// Inputs:
// - bounds_to_world: 4x4 OBB matrix (see geometry.md)
// - view: 4x4 view matrix

// 1. Combine transforms to get bounds-to-view matrix
bounds_to_view = view * bounds_to_world;

// 2. Transform the AABB center to view space
center_vs = bounds_to_view[3].xyz; // translation component

// 3. Compute conservative view-space extents
//    - Take the absolute value of the upper 3x3 part of the matrix (rotation, scale, shear)
//    - Multiply each column by the corresponding bounds-space extent
abs_rot_scale = Abs3x3(bounds_to_view); // Each element is abs(matrix[i][j])
extents_vs = abs_rot_scale * vec3(1, 1, 1);

// 4. The view-space AABB is defined by center_vs and extents_vs
```

**Explanation:**
- The OBB is represented as a 4x4 matrix as described in geometry.md. The first three columns encode the half-axes (scaled by extents), and the fourth column is the center.
- `Abs3x3` means taking the absolute value of each element in the 3x3 rotation/scale/shear part of the matrix.
- Multiplying this matrix by the bounds-space extents gives the maximum reach of the transformed box along each view-space axis, conservatively enclosing the oriented box.

For more details, see [Converting OBB to AABB in Target Space](https://madmann91.github.io/2024/02/10/converting-oriented-bounding-boxes-to-axis-aligned-ones.html) and the geometry.md documentation.

**2. Frustum Plane Extraction**
Obtain the camera's six frustum planes in view space (left, right, top, bottom, near, far), typically via `camera.get_frustum_planes()`. Because these planes are defined in view space, their orientation and position are fixed relative to the camera axes, simplifying intersection tests and allowing them to be efficiently reused or cached on the camera.

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
- Otherwise, the box is at least partially inside the frustum.

This approach is efficient and SIMD-friendly, and is commonly known as the "slab method" for AABB-plane intersection.

---

## Usage Example

```cpp
// Assume draw_data_manager is up to date for the current frame
const auto& mesh_draw_cull_data = draw_data_manager.get_mesh_draw_cull_data();

// Perform culling for the current camera
std::vector<uint32_t> visible_draws = culling_system.cull(camera, mesh_draw_cull_data);

// Iterate over visible draws for rendering
for (uint32_t draw_idx : visible_draws) {
    const auto& draw_data = draw_data_manager.get_mesh_draw_data()[draw_idx];
    // ... issue draw call using draw_data ...
}
```

- The `cull` function returns indices of visible mesh draws, which can be used to index into all per-draw arrays (draw data, culling data, etc.) owned by the DrawDataManager.
- This approach enables efficient, cache-friendly rendering of only visible objects each frame.

---

### References
- [Real-Time Rendering, 4th Edition](https://www.realtimerendering.com/) – Section on frustum culling
- [OGRE3D Frustum Culling](https://ogrecave.github.io/ogre/api/latest/classOgre_1_1Frustum.html)
- [Converting OBB to AABB in Target Space](https://madmann91.github.io/2024/02/10/converting-oriented-bounding-boxes-to-axis-aligned-ones.html)
- [Geometry Primitives and Operations](../utils/geometry.md)

---

This document should be updated as the culling algorithm evolves.
