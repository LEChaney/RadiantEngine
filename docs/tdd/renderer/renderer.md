# Renderer Module Design Document

## Summary Table

| Responsibility                | Owned/Managed by Renderer | Delegated To                   |
|-------------------------------|:-------------------------:|--------------------------------|
| Main rendering loop           | Yes                       |                                |
| Global descriptor sets        | Yes                       |                                |
| Material descriptor sets      | No                        | MaterialSystem                 |
| Pipeline/layout metadata      | No                        | PipelineManager                |
| Texture resources             | No                        | TextureManager                 |
| Draw/culling data             | No                        | DrawDataManager, CullingSystem |

---

## 1. Purpose

The renderer orchestrates the rendering pipeline, manages the main loop, global descriptor sets, and coordinates draw calls, culling, and resource binding between systems.

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
- **Respond to active scene changes as an observer of the SceneManager. When notified of a new set of active scenes, the Renderer coordinates the clearing and repopulation of draw and culling data from those scenes.**

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

## 3a. Vulkan Initialization and Swapchain Setup

### VulkanContext Initialization
- Responsible for creating and managing the Vulkan instance, physical and logical devices, and the rendering surface.
- Handles Vulkan API version selection, validation layers, and device feature queries.
- Manages device selection (discrete GPU preferred), queue family discovery, and logical device creation.
- Creates the Vulkan surface (via windowing system integration, e.g., SDL or GLFW).
- Cleans up all Vulkan objects on shutdown.

### SwapchainManager Setup
- Handles creation, recreation, and destruction of the Vulkan swapchain.
- Selects optimal surface format, present mode, and swapchain extent based on window and device capabilities.
- Manages swapchain images, image views, framebuffers, and synchronization primitives (semaphores, fences).
- Responds to window resize and surface changes by recreating the swapchain and all dependent resources.
- Provides APIs for acquiring the next image, presenting, and handling out-of-date/resize events.

### Integration and Error Handling
- VulkanContext and SwapchainManager are initialized during renderer startup.
- On window resize or surface loss, the renderer triggers swapchain recreation via SwapchainManager.
- All Vulkan errors are checked and reported; swapchain recreation is robust to minimize frame drops.
- The renderer integrates swapchain image acquisition and presentation into the main frame loop.

#### Example Initialization Flow
```cpp
// During renderer startup:
vulkan_context.initialize(window_handle);
swapchain_manager.create_swapchain(vulkan_context, window_extent);

// Per frame:
swapchain_manager.acquire_next_image();
// ...record and submit draw commands...
swapchain_manager.present_image();

// On window resize:
swapchain_manager.recreate_swapchain(new_extent);
```

---

---

## 4. Key Classes & Public API

### Renderer (Public API)
- set_camera(Camera*) ([see camera design document](camera.md))
- render_frame()
  - Renders the current scene(s) from the perspective of the current camera ([see camera design document](camera.md)).
  - Handles all internal steps (frame begin/end, culling, draw call recording, presentation, etc.)
- on_active_scenes_changed(const std::vector<SceneHandle>& new_active_scenes)
  - Observer callback, called by the SceneManager when the set of active scenes changes. The Renderer must clear and repopulate all draw and culling data from the new active scenes.

#### Scene Switching and Data Repopulation
- The renderer registers as an observer with the SceneManager and responds to `on_active_scenes_changed` notifications.
- When notified, the Renderer:
  - Clears all mesh draw data and culling/bounds data by calling `clear_all_data()` on both the DrawDataManager and CullingSystem.
  - Repopulates these systems by calling `populate_from_scene(Scene*)` for each new active scene.
  - Ensures that only the current active scene(s)' data is used for culling and rendering.

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
  - Handles efficient visibility determination of scene objects before rendering.
  - Maintains and updates per-draw culling data, and exposes APIs for culling and data management.
  - See [Culling System Design](renderer/culling_system.md) for full details.

- **DrawDataManager**
  - Owns and manages all draw data and related arrays.
  - Provides APIs for clearing all draw data (`clear_all_data()`) and repopulating from a scene (`populate_from_scene(Scene*)`).
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

- **Scene Activation:** The Renderer registers as an observer of the SceneManager. When the set of active scenes changes, the Renderer is notified via `on_active_scenes_changed`, and it clears and repopulates all draw and culling data from the new active scenes.
- **begin_frame()**: Prepares the renderer for a new frame (acquire swapchain image, reset command buffers, etc.)
- **end_frame()**: Finalizes and submits the frame (submit command buffers, handle synchronization, and prepare for presentation)
- **perform_culling()**: Uses the CullingSystem to determine visible mesh draws for the current frame. The CullingSystem maintains its own per-draw culling data, and uses the camera's cached frustum planes and view matrix. Frustum planes are only updated when the camera's projection matrix changes.
- **perform_sorting()**: Sorts visible draw calls for efficiency (e.g., to minimize state changes or for correct transparency rendering).
- **record_draw_commands()**: Records Vulkan draw commands for the current frame (uses ResourceManager, PipelineManager, DescriptorManager)
- **present_frame()**: Presents the rendered image to the screen (submits the present request to the swapchain)
- **Resource Ownership**: Scenes (or SceneManager) are responsible for tracking and releasing all GPU resources (images, buffers, samplers, descriptor sets) they use. ResourceManager and DescriptorManager only allocate and destroy resources on request.
- **Scene Switching and Data Repopulation:** When notified of active scene changes, the renderer calls `clear_all_data()` and then `populate_from_scene(new_scene)` on both the DrawDataManager and CullingSystem for each active scene. This ensures that only the current scene(s)' data is used for culling and rendering, and that all per-draw and per-cull data is up to date.

---

## 7. Handling Culling

- Renderer owns the CullingSystem.
- After scene transforms are updated and synchronized, the renderer calls `culling_system.cull(camera)` to determine the set of visible mesh draws for the current frame.
- The CullingSystem maintains its own tightly-packed per-draw culling data, which is synchronized with the scene after transform changes via `sync_transforms`.
- The CullingSystem uses the camera's cached frustum planes and view matrix. Frustum planes are only recalculated if the camera's projection matrix has changed since the last frame.
- The renderer uses the list of visible draw indices returned by the CullingSystem to issue draw calls for only visible objects.
- See [Culling System Design](renderer/culling_system.md) for further details and API.

---

## 8. Example Usage

### Public API Example

```cpp
// Application code:
scene_manager.add_observer(&renderer);
scene_manager.set_active_scene(scene_handle);
renderer.set_camera(&camera); // [see camera design document](camera.md)

// Per frame:
renderer.render_frame();

// On window resize:
renderer.resize_swapchain(new_width, new_height);
// (renderer handles frustum culler update internally)
```

---

## 9. Internal Usage Example (For Maintainers)

```cpp
// Internal renderer workflow (simplified):
void Renderer::on_active_scenes_changed(const std::vector<SceneHandle>& new_active_scenes) {
    draw_data_manager.clear_all_data();
    culling_system.clear_all_data();
    for (SceneHandle handle : new_active_scenes) {
        Scene* scene = scene_manager.get_scene(handle);
        draw_data_manager.populate_from_scene(scene);
        culling_system.populate_from_scene(scene);
    }
}

void Renderer::render_frame() {
    begin_frame();
    auto visible_draws = culling_system.cull(camera); // Uses camera's cached frustum planes and view matrix
    perform_sorting(visible_draws); // Sorts visible objects for batching/state changes
    record_draw_commands(visible_draws); // Uses ResourceManager, PipelineManager, DescriptorManager
    end_frame();
    present_frame();
}

// Example of culling and resource usage:
void Renderer::perform_culling() {
    auto visible_draws = culling_system.cull(camera);
    // visible_draws contains indices of visible mesh draws for this frame
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
- [GLM documentation](https://github.com/g-truc/glm/)

---

## Coordination with Core System

The workflow for transform updates, system synchronization, and the renderer loop is now documented in [Core System Coordination](../core/core_system_coordination.md). Refer to that document for the high-level sequence and rationale. This section only summarizes the renderer's role:

- The renderer must be called after all system synchronizations are complete.
- It assumes all per-system data is up to date for the current frame.
- The renderer is an observer of the SceneManager and responds to active scene changes, rather than owning this state itself.

---

## Global Descriptor Set Management

- The renderer is responsible for creating, updating, and binding global (shared) descriptor sets each frame, typically at set 0, containing data such as camera parameters, light data, and other per-frame or per-scene resources.
- The renderer queries the PipelineManager for the required descriptor set layouts for each pipeline layout, ensuring that all required sets are bound before issuing draw calls.
- The renderer binds global descriptor sets (set 0) and material descriptor sets (set 1+) for each draw call.
- This workflow ensures that all shaders have access to both global and material data, and that descriptor set binding is consistent and efficient across all pipelines.

---

## Dependency Injection and System Dependencies

The Renderer depends on several core systems, which should be injected or registered explicitly to enable modularity, testability, and decoupling. All dependencies should be provided via constructor parameters, setter methods, or explicit registration APIs.

### Renderer Dependencies
- **RHI (RHIBase/IRHI)**: Injected via constructor or setter. All rendering API calls are made through this abstraction. The RHI can be a real backend (Vulkan, Metal, etc.) or a mock for testing.
- **DrawDataManager**: Injected via constructor or setter. Used for managing draw data.
- **CullingSystem**: Injected via constructor or setter. Used for culling visible objects.
- **PipelineManager**: Injected via constructor or setter. Used for pipeline state management.
- **TextureManager**: Injected via constructor or setter. Used for texture resource management.
- **MaterialSystem**: Injected via constructor or setter. Used for material descriptor sets.
- **SceneManager**: Registered as an observer to receive active scene changes.
- **Camera**: Set via `set_camera()`.

#### Example (C++)
```cpp
Renderer(RHIBase& rhi,
         DrawDataManager* draw_data_mgr,
         CullingSystem* culling_sys,
         PipelineManager* pipeline_mgr,
         TextureManager* texture_mgr,
         MaterialSystem* material_sys,
         SceneManager* scene_mgr);

void set_camera(Camera* camera);
```

- All dependencies, including the RHI, can be replaced with mocks or test doubles for unit testing.
- The Renderer does not create or own these systems; it only uses them via interfaces or pointers.

---

This document should be updated as the renderer module evolves.
