# Renderer Module Design Document

## 1. Purpose

The renderer module is responsible for orchestrating the rendering pipeline, managing GPU resources, handling draw calls, culling, and coordinating the interaction between the camera ([see camera design document](camera.md)), scene, and Vulkan API. It serves as the central hub for all rendering operations, including managing the active scene and ensuring all per-frame data is up to date.

---

## 2. Responsibilities

- Manage the main rendering loop and frame lifecycle
- Handle Vulkan resource creation and destruction (swapchain, images, buffers, pipelines, etc.)
- Coordinate with the camera to obtain view/projection matrices
- Perform frustum culling and visibility determination
- Sort and batch draw calls for efficiency
- Manage descriptor sets and pipeline state
- Handle window resizing and swapchain recreation
- Integrate with UI (e.g., ImGui) and post-processing
- **Track the active scene and coordinate the clearing and repopulation of draw and culling data when the scene changes**

---

## 3. Structure

The renderer module consists of the following main files and folders:

- src/renderer/Renderer.h / Renderer.cpp
- src/renderer/VulkanContext.h / VulkanContext.cpp
- src/renderer/SwapchainManager.h / SwapchainManager.cpp
- src/renderer/FrustumCuller.h / FrustumCuller.cpp
- src/renderer/ResourceManager.h / ResourceManager.cpp
- src/renderer/PipelineManager.h / PipelineManager.cpp
- src/renderer/DescriptorManager.h / DescriptorManager.cpp
- src/renderer/DrawDataManager.h / DrawDataManager.cpp
- src/renderer/CullingSystem.h / CullingSystem.cpp

---

## 4. Key Classes & Public API

### Renderer (Public API)
- setScene(Scene*)
- setCamera(Camera*) ([see camera design document](camera.md))
- renderFrame()
  - Renders the current scene from the perspective of the current camera ([see camera design document](camera.md)).
  - Handles all internal steps (frame begin/end, culling, draw call recording, presentation, etc.)
- switchScene(Scene*)
  - Switches the active scene pointer and coordinates all necessary data repopulation for rendering.

#### Scene Switching and Data Repopulation
When the active scene is switched, the Renderer is responsible for:
- Clearing all mesh draw data and culling/bounds data by calling `clearAllData()` on both the DrawDataManager and CullingSystem.
- Repopulating these systems by calling `populateFromScene(Scene*)` with the new scene.
- Ensuring that only the current scene's data is used for culling and rendering.

---

## 5. Internal Classes & Responsibilities

> **Note:** The following classes are internal to the renderer module and are not part of the public API.

- **VulkanContext**
  - Encapsulates Vulkan instance, physical/logical device, and surface.
  - Responsible for Vulkan initialization, device selection, and cleanup.
  - Provides access to Vulkan handles needed by other internal classes.

- **SwapchainManager**
  - Handles creation, recreation, and destruction of the Vulkan swapchain.
  - Manages swapchain images, image views, and synchronization objects.
  - Responds to window resize and surface changes.

- **CullingSystem**
  - Responsible for efficient visibility determination of scene objects (meshes, mesh draws, etc.) before rendering.
  - Maintains tightly-packed per-draw culling data (world transforms, bounds) for fast, cache-friendly iteration and SIMD-friendly culling algorithms.
  - Synchronizes culling data with the scene graph after transform changes via `syncTransforms`.
  - Provides APIs for clearing all culling data (`clearAllData()`) and repopulating from a scene (`populateFromScene(Scene*)`).
  - The Renderer is responsible for invoking these methods when the scene changes.
  - Remembers the last set of frustum planes, updating them only when the camera's projection matrix changes.
  - The `cull` method takes a `Camera&` and uses the camera's cached frustum planes and view matrix internally.
  - Exposes APIs for updating transforms and bounds, and for reporting visible mesh draws for rendering.
  - See [Culling System Design](renderer/culling_system.md) for details.

- **DrawDataManager**
  - Owns and manages all draw data and related arrays.
  - Provides APIs for clearing all draw data (`clearAllData()`) and repopulating from a scene (`populateFromScene(Scene*)`).
  - The Renderer is responsible for invoking these methods when the scene changes.
  - Provides APIs for synchronizing transforms, updating GPU buffers, and querying draw data.

- **ResourceManager**
  - Allocates and destroys GPU resources (buffers, images, samplers) on request.
  - Does **not** own or track the lifetime of resources; scenes (or scene manager) are responsible for tracking and releasing resources they use.

- **PipelineManager**
  - Manages graphics and compute pipelines globally.
  - Handles pipeline creation, caching, and destruction.
  - Pipelines are referenced by scenes as needed, but are owned and destroyed by the PipelineManager.

- **DescriptorManager**
  - Allocates and recycles descriptor sets.
  - Does **not** own or track descriptor set lifetime; scenes are responsible for freeing descriptor sets they allocate.

---

## 6. Internal Structure & Workflow (For Maintainers)

- **beginFrame()**: Prepares the renderer for a new frame (acquire swapchain image, reset command buffers, etc.)
- **endFrame()**: Finalizes and submits the frame (submit command buffers, handle synchronization, and prepare for presentation)
- **performCulling()**: Uses the CullingSystem to determine visible mesh draws for the current frame. The CullingSystem maintains its own per-draw culling data, and uses the camera's cached frustum planes and view matrix. Frustum planes are only updated when the camera's projection matrix changes.
- **performSorting()**: Sorts visible draw calls for efficiency (e.g., to minimize state changes or for correct transparency rendering).
- **recordDrawCommands()**: Records Vulkan draw commands for the current frame (uses ResourceManager, PipelineManager, DescriptorManager)
- **presentFrame()**: Presents the rendered image to the screen (submits the present request to the swapchain)
- **Resource Ownership**: Scenes (or SceneManager) are responsible for tracking and releasing all GPU resources (images, buffers, samplers, descriptor sets) they use. ResourceManager and DescriptorManager only allocate and destroy resources on request.
- **Scene Switching and Data Repopulation**: When switching scenes, the renderer calls `clearAllData()` and then `populateFromScene(newScene)` on both the DrawDataManager and CullingSystem. This ensures that only the current scene's data is used for culling and rendering, and that all per-draw and per-cull data is up to date.

---

## 7. Handling Culling

- Renderer owns the CullingSystem.
- After scene transforms are updated and synchronized, the renderer calls `cullingSystem.cull(camera)` to determine the set of visible mesh draws for the current frame.
- The CullingSystem maintains its own tightly-packed per-draw culling data, which is synchronized with the scene after transform changes via `syncTransforms`.
- The CullingSystem uses the camera's cached frustum planes and view matrix. Frustum planes are only recalculated if the camera's projection matrix has changed since the last frame.
- The renderer uses the list of visible draw indices returned by the CullingSystem to issue draw calls for only visible objects.
- See [Culling System Design](renderer/culling_system.md) for further details and API.

---

## 8. Example Usage

### Public API Example

```cpp
// Application code:
renderer.setScene(&scene);
renderer.setCamera(&camera); // [see camera design document](camera.md)

// Per frame:
renderer.renderFrame();

// On window resize:
renderer.resizeSwapchain(newWidth, newHeight);
// (renderer handles frustum culler update internally)
```

---

## 9. Internal Usage Example (For Maintainers)

```cpp
// Internal renderer workflow (simplified):
void Renderer::switchScene(Scene* newScene) {
    setScene(newScene);
    drawDataManager.clearAllData();
    cullingSystem.clearAllData();
    drawDataManager.populateFromScene(newScene);
    cullingSystem.populateFromScene(newScene);
}

void Renderer::renderFrame() {
    beginFrame();
    auto visibleDraws = cullingSystem.cull(camera); // Uses camera's cached frustum planes and view matrix
    performSorting(visibleDraws); // Sorts visible objects for batching/state changes
    recordDrawCommands(visibleDraws); // Uses ResourceManager, PipelineManager, DescriptorManager
    endFrame();
    presentFrame();
}

// Example of culling and resource usage:
void Renderer::performCulling() {
    auto visibleDraws = cullingSystem.cull(camera);
    // visibleDraws contains indices of visible mesh draws for this frame
}
```

---

## 10. Future Extensions

- Support for advanced rendering features (meshlets, ray tracing, DDGI, etc.)
- Integration with post-processing and UI overlays
- Multi-threaded rendering and resource streaming

---

## 11. References
- [Vulkan specification](https://www.khronos.org/registry/vulkan/)
- [GLM documentation](https://github.com/g-truc/glm)

---

## Coordination with Core System

The workflow for transform updates, system synchronization, and the renderer loop is now documented in [Core System Coordination](../core/core_system_coordination.md). Refer to that document for the high-level sequence and rationale. This section only summarizes the renderer's role:

- The renderer must be called after all system synchronizations are complete.
- It assumes all per-system data is up to date for the current frame.

---

This document should be updated as the renderer module evolves.
