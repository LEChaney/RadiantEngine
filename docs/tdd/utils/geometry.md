# Geometry Primitives and Operations

## Bounding Volumes
### AABB (Axis-Aligned Bounding Box)
- Defined by a center point and extents (half-widths) along each axis in 3D space.
- The minimum and maximum corners can be computed as `center - extents` and `center + extents`.
- Sides remain aligned with the coordinate axes.
- This representation often simplifies mathematical operations.
- **Example struct definition (C++):**
    ```cpp
    struct AABB {
        glm::vec3 center;
        glm::vec3 extents; // half-widths along each axis
    };
    ```

### OBB (Oriented Bounding Box)
- Represented by a 4x4 (or 3x4) transformation matrix `M` that maps "bounds space" (AABB centered at (0, 0, 0), extents [-1, 1]) to the target space.
- The first three columns of `M` are half-axis vectors (encoding orientation and half-lengths); the fourth column is the center position in the target space.
- Any point inside the OBB can be expressed as `P = M * vec4(v, 1)`, where `v` is a bounds-space point with components in [-1, 1].
- **Example struct definition (C++):**
    ```cpp
    using OBB = mat4;
    ```

## Basic Primitives
- **Points:** Single locations in 3D space, typically represented as 3D vectors.
- **Lines:** Defined by two points or a point and a direction vector.
- **Planes:** Defined by a point and a normal vector, or by the plane equation `ax + by + cz + d = 0`.

## Intersection & Query Helpers
- **Line intersections:** Functions to test if and where lines, rays, or segments intersect with other primitives (planes, boxes, etc.).
- **Shape intersections:** Tests for overlap between bounding volumes (AABB vs OBB, OBB vs OBB, etc.).
- **Containment tests:** Check if a point or shape is inside another shape.

## Usage Examples
- Example: Constructing an OBB from mesh data and testing point containment.
- Example: Using intersection helpers to perform ray picking in a scene.

## References
- [Converting OBB to AABB in Target Space](https://madmann91.github.io/2024/02/10/converting-oriented-bounding-boxes-to-axis-aligned-ones.html)