# Core System Coordination: Initialization, Main Loop, and Teardown

## 1. Purpose

This document describes the high-level orchestration of the engine's main systems, including initialization, per-frame coordination, and teardown. It defines the responsibilities of the core engine loop, how it coordinates subsystems (scene, renderer, camera, asset management, etc.), and the correct order of operations for transform propagation, system synchronization, and rendering. This is the authoritative reference for system-level workflow and lifecycle, and should be cross-referenced by all module-level documentation.

---

## 2. Responsibilities of the Core Engine

- Initialize all major subsystems (windowing, Vulkan, renderer, scene, camera, asset loaders, etc.)
- Manage the main loop, including per-frame updates and rendering
- Coordinate transform propagation and system synchronization
- Handle input, events, and scene switching
- Manage resource and system teardown on shutdown

---

## 3. System Initialization Sequence

1. **Window and Platform Initialization**
   - Create the application window and initialize platform-specific services (e.g., SDL, GLFW).
2. **Vulkan Context Initialization**
   - Create Vulkan instance, select physical device, create logical device and surface.
3. **Subsystem Initialization**
   - Initialize:
     - Renderer (with Vulkan context)
     - SceneManager and initial Scene(s)
     - Camera(s)
     - AssetManager/Loader
     - Other systems (UI, input, physics, etc.)
4. **Resource Loading**
   - Load assets (models, textures, shaders) and populate the scene graph.

---

## 4. Main Loop Coordination

The main loop is responsible for coordinating all per-frame updates. The correct order of operations is essential for correctness and performance. The recommended workflow for each frame is:

1. **Input and Game Logic**
   - Poll input events and pass them to relevant systems, including the camera (e.g., `camera.processSDLEvent(event)`).
   - Update game/application logic, which may include modifying scene node transforms or camera targets.
2. **Scene/Node Updates**
   - Apply any changes to scene node transforms (e.g., via `scene.setNodeTransform`).
3. **Transform Propagation**
   - Call `scene.updateWorldTransforms()` to propagate local-to-world transforms for all dirty nodes and their descendants. This updates world transforms and populates the set of changed nodes.
4. **System Synchronization (including Camera Update)**
   - For each system that depends on transforms (e.g., CullingSystem, DrawDataManager), call `system.syncTransforms(scene.getChangedNodes(), scene)`. This updates per-system data for all changed nodes.
   - **Call `camera.update(deltaTime)` after transform propagation, so that if the camera is attached to a scene node, it uses up-to-date world transforms.**
   - If the camera is used for rendering, ensure it is set on the renderer (e.g., `renderer.setCamera(&camera)`) before rendering. This may be done once at initialization or whenever the active camera changes.
5. **Rendering**
   - Call `renderer.renderFrame()`. The renderer uses the current camera's view and projection matrices, performs culling, sorting, and draw call submission using up-to-date per-system data.
6. **Cleanup**
   - Call `scene.clearChangedNodes()` to reset the changed set for the next frame.
7. **Other Per-Frame Tasks**
   - UI updates, profiling, etc.

### Example Main Loop (Pseudocode)
```cpp
while (!shouldQuit) {
    pollInputEvents();
    camera.processSDLEvent(event); // Pass input to camera
    updateGameLogic();

    // Scene/node updates
    scene.setNodeTransform(node, newLocalTransform);
    // ... other node updates ...

    // Transform propagation
    scene.updateWorldTransforms();

    // System synchronization
    cullingSystem.syncTransforms(scene.getChangedNodes(), scene);
    drawDataManager.syncTransforms(scene.getChangedNodes(), scene);
    // ... other system syncs ...

    camera.update(deltaTime); // Update camera state and matrices (after transforms are up to date)

    // Ensure renderer uses the correct camera (if not already set)
    renderer.setCamera(&camera);

    // Rendering
    renderer.renderFrame();

    // Cleanup
    scene.clearChangedNodes();
}
```

---

## 5. System Teardown Sequence

1. **Wait for GPU Idle**
   - Ensure all GPU work is complete (e.g., `vkDeviceWaitIdle`).
2. **Destroy Renderer and Vulkan Resources**
   - Destroy swapchain, pipelines, descriptor sets, and all GPU resources.
3. **Destroy Subsystems**
   - Destroy SceneManager, Camera, AssetManager, and other systems.
4. **Destroy Window and Platform Resources**
   - Destroy the application window and platform-specific services.

---

## 6. Cross-Module Coordination

- **Camera System**: The camera processes input events early in the frame, but its `update()` method must be called after scene transforms are up to date (i.e., after `scene.updateWorldTransforms()` and all `syncTransforms` calls). This ensures that, if the camera is attached to a scene node, it uses the latest world transform for its matrices. The renderer must be set to use the current camera before rendering. If the camera is attached to a scene node, it will follow that node's world transform automatically during its update.
- **Scene and Renderer**: The scene module is responsible for transform propagation and tracking changed nodes. The renderer and other systems must synchronize their data after transforms are updated and before rendering.
- **System Data Ownership**: Each system owns its per-node data and is responsible for updating it in response to scene changes.
- **Explicit Synchronization**: No system should assume transforms or per-node data are up to date except immediately after the main loop's synchronization phase.

---

## 7. References and Cross-Links

- [High-Level Design](../high_level_design.md)
- [Scene Module Design](../scene/scene.md)
- [Renderer Module Design](../renderer/renderer.md)
- [Camera Module Design](../camera/camera.md)

---

This document should be updated as the engine's architecture evolves. All module-level documentation should reference this document for system-level coordination and workflow.
