# Camera Module Design Document

## 1. Purpose

The camera module is responsible for representing and manipulating the view and projection of the virtual camera in the renderer. It provides interfaces for camera movement, orientation, and supporting both user and programmatic control. **For simplicity, input handling (e.g., SDL events) is handled directly by the Camera class, rather than a separate controller.**

---

## 2. Responsibilities

- Store camera parameters (position, orientation, FOV, aspect ratio, near/far planes)
- Compute and provide view and projection matrices
- Support camera movement and rotation (e.g., FPS, orbit, or custom controls)
- **Directly handle input events (e.g., SDL events) for interactive control**
- Respond to viewport size changes to update projection
- **Optionally attach to a scene node to follow its world transform, supporting camera tracking, third-person, or cutscene behaviors**

---

## 3. Structure

- **src/camera/Camera.h / Camera.cpp**: Core camera logic, math, state, input handling, and node attachment logic

---

## 4. Key Classes & Functions

### Camera
- Fields:
  - position, orientation (quaternion or Euler), FOV, aspect, near, far, targetPosition, targetOrientation
  - **Optional: attached Scene* and NodeHandle, and local offset transform if attached**
- Methods:
  - set/get position & orientation
  - set/get projection parameters
  - setViewportSize(width, height) (called on window resize)
  - getViewMatrix()
  - getProjectionMatrix()
  - update(deltaTime):
    - If attached to a node, queries the node's world transform from the scene and applies the local offset to compute position/orientation
    - Otherwise, smoothly interpolates position/orientation toward target values
    - Recalculates view and projection matrices as needed
  - **processSDLEvent(SDL_Event&): Handles input events directly (movement, mouse, etc.)**
  - **attachToNode(Scene* scene, NodeHandle node, const glm::mat4& localOffset = glm::mat4(1.0f)): Attaches the camera to a scene node, with optional local offset**
  - **detachFromNode(): Detaches the camera from any node, resuming independent control**
  - **isAttached() const: Returns true if the camera is currently attached to a node**
  - **getAttachedNode() const: Returns the currently attached node handle (or INVALID_HANDLE if not attached)**
  - **getLocalOffset() const: Returns the local offset transform used when attached**

---

## 5. Design Principles

- **Simplicity**: Input handling is performed directly by the Camera class to reduce indirection and complexity.
- **Testability**: Camera logic can still be unit tested without renderer or input.
- **Extensibility**: If needed, input handling can be separated in the future, but is not required for current needs.
- **Flexibility**: Node attachment allows the camera to follow scene objects for tracking, third-person, or cutscene-style behaviors, with optional local offset.

---

## 6. Handling Viewport Size Changes

- The camera's projection matrix depends on the viewport's aspect ratio.
- On window resize, the renderer or main loop should call `camera.setViewportSize(newWidth, newHeight)`.
- This updates the aspect ratio and triggers a recalculation of the projection matrix.
- The camera should mark its projection as dirty and update it on the next `update()` or immediately.
- All dependent systems should use the updated projection matrix after resize.

---

## 7. Example Usage

```cpp
// Attach camera to a node (e.g., for third-person or cutscene)
camera.attachToNode(&scene, playerNode, glm::translate(glm::vec3(0, 2, -5)));

// In main loop or scene update:
camera.processSDLEvent(event); // handles input and updates camera state
camera.update(deltaTime); // Camera follows node's world transform if attached, or interpolates otherwise

// Detach camera for free movement
camera.detachFromNode();

// On window resize:
camera.setViewportSize(newWidth, newHeight);

// For rendering:
mat4 view = camera.getViewMatrix();
mat4 proj = camera.getProjectionMatrix();
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
  - Use GLM's lookAt or equivalent as a reference implementation. Given a known position and orientation, verify that `getViewMatrix()` returns a matrix matching GLM's output within a small epsilon.
  - Test edge cases (e.g., identity orientation, 90-degree rotations).
  - Property-based: Confirm that the view matrix inverts the camera's transform (i.e., transforming a point to camera space and back yields the original point).

- **Projection Matrix Calculation**
  - Use GLM's perspective/ortho as a reference. Given known FOV, aspect, near, and far values, verify that `getProjectionMatrix()` matches GLM's output within a small epsilon.
  - Test for both perspective and orthographic projections (if supported).
  - Property-based: Check that the projection matrix maps the frustum center to the expected clip space coordinate.

- **Viewport Resize Handling**
  - After calling `setViewportSize` with new dimensions, verify that the aspect ratio and projection matrix are updated correctly.

- **Camera Movement and Smoothing**
  - Set a target position/orientation, call `update(deltaTime)`, and verify that the camera interpolates toward the target as expected.
  - Test with different `deltaTime` values for stability.

- **Parameter Setters/Getters**
  - Verify that set/get methods for position, orientation, FOV, etc., work as intended and maintain internal consistency.

- **Node Attachment and Following**
  - Attach the camera to a scene node with a known world transform and local offset. After calling `update()`, verify that the camera's computed position and orientation match the expected result (node's world transform combined with the local offset).
  - Move or animate the node, call `update()`, and verify that the camera continues to follow the node correctly.
  - Detach the camera and ensure it retains its last computed world transform and resumes independent movement.
  - Attach and detach repeatedly to different nodes, verifying correct behavior each time.
  - Test with various local offsets (identity, translation, rotation) to ensure correct application.
  - If input is allowed while attached, verify that input affects the local offset (if supported/configured).

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
  - Test with very small or large FOV and aspect ratios to ensure the projection matrix does not produce invalid results.

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
