# Vulkan Renderer Refactor: High-Level Technical Design Document

## 1. Overview

This document describes the proposed refactoring of the Vulkan renderer codebase. The goal is to improve modularity, maintainability, testability, and extensibility while preserving existing functionality.

---

## 2. Current State

- **Monolithic source files**: Core logic (Vulkan setup, camera, pipelines, etc.) is implemented in large, tightly coupled files.
- **Direct resource management**: Manual handling of Vulkan resources, with limited abstraction.
- **Limited separation of concerns**: Rendering, input, and scene management are not clearly separated.
- **Minimal testing**: No clear boundaries for unit or integration testing.

---

## 3. Refactored Architecture

### 3.1. High-Level Structure

- **src/**
  - [`core/`](core/) – Engine core (initialization, main loop, logging)
  - [`renderer/`](renderer/renderer.md) – Vulkan abstraction (device, swapchain, pipelines, descriptors)
  - [`scene/`](scene/scene.md) – Scene graph and node hierarchy (see [Scene Module Design](scene/scene.md) for details)
  - [`camera/`](camera/camera.md) – Camera logic, controls, and input handling
  - [`assets/`](assets/) – Asset loading (models, textures, shaders)
  - [`utils/`](utils/) – Math, helpers, utilities

### 3.2. Key Modules

#### 3.2.1. Core Engine

- **Engine**: Entry point, manages main loop, delegates to subsystems.
- **Logger**: Centralized logging facility.

#### 3.2.2. Renderer

- See [Renderer Module Design](renderer/renderer.md) for details.
- **VulkanContext**: Encapsulates Vulkan instance, device, and surface.
- **SwapchainManager**: Handles swapchain creation and recreation.
- **PipelineManager**: Manages graphics and compute pipelines globally. Pipelines are referenced by scenes as needed, but are owned and destroyed by the PipelineManager.
- **DescriptorManager**: Allocates and recycles descriptor sets. Scenes are responsible for freeing descriptor sets they allocate.
- **ResourceManager**: Allocates and destroys GPU resources (buffers, images, samplers) on request. Does not own or track resource lifetime; scenes (or scene manager) are responsible for tracking and releasing resources they use.
- **FrustumCuller**: Frustum plane extraction and culling logic.

#### 3.2.3. Scene

- **Node**: Base class for scene graph nodes, with transform and hierarchy. See [Scene Module Design](scene/scene.md).
- **MeshNode**: Node with mesh reference for rendering. See [Scene Module Design](scene/scene.md#key-classes--public-api).
- **LightNode** (future): Node with lighting information. See [Scene Module Design](scene/scene.md#future-extensions).
- **SceneGraph**: Hierarchical structure for nodes. See [Scene Module Design](scene/scene.md#internal-structure--workflow-for-maintainers).

#### 3.2.4. Camera

- See [Camera Module Design](camera/camera.md) for details.
- **Camera**: Stores view/projection, frustum, movement logic, and handles input events directly (e.g., SDL events).
- **FrustumCuller**: Extracts and tests frustum planes (see Renderer for implementation).

#### 3.2.5. Assets

- **AssetLoader**: Loads models, textures, and shaders.
- **ShaderManager**: Compiles and manages shader modules.

#### 3.2.6. Utilities

- **Math**: Common math functions and types.
- **Helpers**: Miscellaneous utilities.

---

## 4. Design Principles

- **Single Responsibility Principle**: Each class/module has a clear, focused purpose.
- **Encapsulation**: Hide Vulkan and SDL details behind clean interfaces.
- **Testability**: Decouple logic for easier unit and integration testing.
- **Extensibility**: Facilitate adding new features (e.g., new rendering techniques, input devices).
- **Documentation**: Each module/class is documented with usage and design rationale.
- **Explicit Resource Ownership**: ResourceManager and DescriptorManager act as allocators/services only. Scene/SceneManager owns and tracks all GPU resources it uses, and is responsible for releasing them on unload. PipelineManager owns pipelines globally.

---

## 5. Example: Camera Refactor

See [Camera Module Design](camera/camera.md) for more details.

- **camera/Camera.h / Camera.cpp**: Camera math, state, and direct input handling.

**Benefits**:
- Camera logic is testable without SDL.
- Input handling is simple and direct, with no unnecessary abstraction.

---

## 6. Build System

- **CMake**: Modular targets for each subsystem.
- **Third-party dependencies**: Managed in `third_party/`, linked via CMake.

---

## 7. Testing

- **Unit tests**: For math, camera, and utility modules (see [Camera Module Design](camera/camera.md#testing)).
- **Integration tests**: For renderer and scene management (see [Scene Module Design](scene/scene.md#example-usage)).
- **Test assets**: Minimal GLTFs and textures for automated tests.

---

## 8. Migration Plan

1. Incrementally extract modules (start with Camera, Renderer, Scene).
2. Write unit tests for extracted modules.
3. Refactor main loop to use new subsystems.
4. Update build scripts.
5. Document new architecture.

---

## 9. Future Work

### Minimum Features
- Meshlet shading and culling
- Ray traced shadows and reflections
- DDGI (Dynamic Diffuse Global Illumination)
- Dynamic PBR sky system

### Stretch Features
- Volumetric fog
- Simple particle system (dust motes, floating leaves) with wind system
- Wind system affecting foliage
- Fur/hair rendering (focus on fur)
- Character skin rendering (focus on animal skin)
- Rigged character animation (simple playback, no complex blending)

---

## 10. Appendix

- **UML diagrams** (to be added as modules are refactored).
- **Code style guide**.
- **Module API documentation**.

---

This document should serve as a living reference for the refactor. Each module should have its own mini-design doc as it is implemented.
