# Camera Module Design Document

## 1. Purpose

The camera module is responsible for representing and manipulating the view and projection of the virtual camera in the renderer. It provides interfaces for camera movement, orientation, and supporting both user and programmatic control. **For simplicity, input handling (e.g., SDL events) is handled directly by the Camera class, rather than a separate controller.**

---

## 2. Responsibilities

- Store camera parameters (position, orientation, FOV, aspect ratio, near/far planes)
- Compute and provide view and projection matrices
- Support camera movement and rotation (e.g., FPS, orbit, or custom controls)
- Respond to viewport size changes to update projection
- **Directly handle input events (e.g., SDL events) for interactive control**
- **Optionally attach to a scene node to follow its world transform, supporting camera tracking, third-person, or cutscene behaviors**

---

## 3. Structure

- **src/camera/Camera.h / Camera.cpp**: Core camera logic, math, state, input handling, and node attachment logic

---

## 4. Key Classes & Functions

### Camera
- Fields:
  - position, orientation (quaternion or Euler), fov, aspect, near, far, target_position, target_orientation
  - Optional: attached Scene* and node_handle, and local_offset transform if attached
  - Cached frustum_planes in camera view space, updated whenever the projection matrix changes
- Methods:
  - set/get position & orientation
  - set/get projection parameters
  - set_viewport_size(width, height) (called on window resize)
  - get_view_matrix()
  - get_projection_matrix()
  - update(delta_time):
    - If attached to a node, queries the node's world transform from the scene and applies the local_offset to compute position/orientation
    - Otherwise, smoothly interpolates position/orientation toward target values
    - Recalculates view and projection matrices as needed
    - If the projection matrix has changed, recalculates and caches the frustum_planes in camera view space
  - get_frustum_planes() const: Returns the cached frustum_planes in camera view space.
  - process_sdl_event(SDL_Event&): Handles input events directly (movement, mouse, etc.)
  - attach_to_node(Scene* scene, node_handle node, const glm::mat4& local_offset = glm::mat4(1.0f)): Attaches the camera to a scene node, with optional local_offset
  - detach_from_node(): Detaches the camera from any node, resuming independent control
  - is_attached() const: Returns true if the camera is currently attached to a node
  - get_attached_node() const: Returns the currently attached node_handle (or INVALID_HANDLE if not attached)
  - get_local_offset() const: Returns the local_offset transform used when attached

---

## 5. Design Principles

- **Simplicity**: Input handling is performed directly by the Camera class to reduce indirection and complexity.
- **Testability**: Camera logic can still be unit tested without renderer or input.
- **Extensibility**: If needed, input handling can be separated in the future, but is not required for current needs.
- **Flexibility**: Node attachment allows the camera to follow scene objects for tracking, third-person, or cutscene-style behaviors, with optional local offset.

---

## 6. Handling Viewport Size Changes

- The camera's projection matrix depends on the viewport's aspect ratio.
 - On window resize, the renderer or main loop should call `camera.set_viewport_size(new_width, new_height)`.
- This updates the aspect ratio and triggers a recalculation of the projection matrix.
- The camera should mark its projection as dirty and update it on the next `update()` or immediately.
- All dependent systems should use the updated projection matrix after resize.

---

## 7. Example Usage

```cpp
// Attach camera to a node (e.g., for third-person or cutscene)
camera.attach_to_node(&scene, player_node, glm::translate(glm::vec3(0, 2, -5)));

// In main loop or scene update:
camera.process_sdl_event(event); // handles input and updates camera state
camera.update(delta_time); // Camera follows node's world transform if attached, or interpolates otherwise

// Access frustum planes for culling or other purposes:
auto frustum_planes = camera.get_frustum_planes();

// Detach camera for free movement
camera.detach_from_node();

// On window resize:
camera.set_viewport_size(new_width, new_height);

// For rendering:
mat4 view = camera.get_view_matrix();
mat4 proj = camera.get_projection_matrix();
```

---

## 8. Future Extensions

- Support for multiple camera types (orthographic, VR, etc.)
- Camera animation and cutscene support
- **If input handling becomes more complex, consider extracting a CameraController in the future.**

---

## 9. Testing

### Unit Tests

- **View Matrix Calculation**
  - Use GLM's lookAt or equivalent as a reference implementation. Given a known position and orientation, verify that `get_view_matrix()` returns a matrix matching GLM's output within a small epsilon.
  - Test edge cases (e.g., identity orientation, 90-degree rotations).
  - Property-based: Confirm that the view matrix inverts the camera's transform (i.e., transforming a point to camera space and back yields the original point).

- **Projection Matrix Calculation**
  - Use GLM's perspective/ortho as a reference. Given known fov, aspect, near, and far values, verify that `get_projection_matrix()` matches GLM's output within a small epsilon.
  - Test for both perspective and orthographic projections (if supported).
  - Property-based: Check that the projection matrix maps the frustum center to the expected clip space coordinate.

- **Viewport Resize Handling**
  - After calling `set_viewport_size` with new dimensions, verify that the aspect ratio and projection matrix are updated correctly.

- **Camera Movement and Smoothing**
  - Set a target position/orientation, call `update(delta_time)`, and verify that the camera interpolates toward the target as expected.
  - Test with different `delta_time` values for stability.

- **Parameter Setters/Getters**
  - Verify that set/get methods for position, orientation, fov, etc., work as intended and maintain internal consistency.

- **Node Attachment and Following**
  - Attach the camera to a scene node with a known world transform and local_offset. After calling `update()`, verify that the camera's computed position and orientation match the expected result (node's world transform combined with the local_offset).
  - Move or animate the node, call `update()`, and verify that the camera continues to follow the node correctly.
  - Detach the camera and ensure it retains its last computed world transform and resumes independent movement.
  - Attach and detach repeatedly to different nodes, verifying correct behavior each time.
  - Test with various local_offsets (identity, translation, rotation) to ensure correct application.
  - If input is allowed while attached, verify that input affects the local_offset (if supported/configured).

- **Frustum Plane Calculation and Caching**
  - After changing the projection matrix (e.g., via set_viewport_size or set/get projection parameters), verify that the frustum_planes are recalculated and cached correctly in camera view space.
  - Call `get_frustum_planes()` and verify the returned planes match the expected frustum for the current projection.
  - Test that the frustum_planes remain unchanged if the projection matrix does not change between updates.

### Integration Tests

- **Input Handling Integration**
  - Simulate input events and verify that the camera’s state is updated correctly.
  - Ensure that after processing input and updating, the camera’s view matrix reflects the intended movement.

- **Projection Consistency**
  - After a sequence of viewport resizes and parameter changes, verify that the projection matrix remains mathematically correct and stable.

- **Node Attachment Integration**
  - In a test scene, attach the camera to a moving node (e.g., a player or animated object). Simulate node movement and verify that the camera tracks the node visually and via API.
  - Switch attachment between nodes at runtime and verify smooth transitions and correct following.
  - Combine node attachment with viewport resizing and input events to ensure robust behavior under all conditions.

### Edge Cases

- **Extreme FOV/Aspect Ratios**
  - Test with very small or large fov and aspect ratios to ensure the projection matrix does not produce invalid results.

- **Zero/Negative Near and Far Planes**
  - Ensure the system handles or rejects invalid near/far plane values gracefully.

### Visual Validation

- Use visual/manual inspection to confirm that camera movement, orientation, and projection behave as expected in the running application. Unit tests are a safety net for regressions and integration errors, but visual validation is the final authority for correctness.

---

## 10. References
- [glm documentation](https://github.com/g-truc/glm)
- [Vulkan spec: Coordinate systems](https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chapters/vertexpostproc.html)

---

This document should be updated as the camera module evolves.
