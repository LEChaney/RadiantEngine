# Technical Design Documents

This folder contains technical design documentation for each major module in the renderer project. Each module has its own subfolder containing a high-level design document (`scene.md`, `renderer.md`, etc.) and, where appropriate, additional detailed documentation for internal classes and subsystems.

## High-Level Design

- [High-Level Design Document](high_level_design.md)
- [Core System Coordination](core/core_system_coordination.md)

## Structure

- [`renderer/`](renderer/renderer.md) — [Renderer module design and internal documentation](renderer/renderer.md)
    - [CullingSystem](renderer/culling_system.md)
    - [DrawDataManager](renderer/drawdata.md)
    - [MeshSystem](renderer/mesh_system.md)
    - [MaterialSystem](renderer/material_system.md)
    - [LightSystem](renderer/light_system.md)
    - [ResourceAllocator](renderer/resource_allocator.md)
    - [Scene Resource Ownership](renderer/scene_resource_ownership.md)
    - [ResourceManager](renderer/renderer.md#resourcemanager)
    - [PipelineManager](renderer/renderer.md#pipelinemanager)
    - [DescriptorManager](renderer/renderer.md#descriptormanager)
    - [VulkanContext](renderer/renderer.md#vulkancontext)
    - [SwapchainManager](renderer/renderer.md#swapchainmanager)
- [`scene/`](scene/scene.md) — [Scene module design and internal documentation](scene/scene.md)
- [`camera/`](camera/camera.md) — [Camera module design and internal documentation](camera/camera.md)
- [`assets/`](assets/gltf_scene_loader.md) — [GLTF Scene Loader Design](assets/gltf_scene_loader.md)
- [`core/`](core/core_system_coordination.md) — [Core system coordination, main loop, and orchestration](core/core_system_coordination.md)
- (Add more as needed: `utils/`, etc.)

Refer to each module's main design document (e.g., [`renderer.md`](renderer/renderer.md)) for an overview and links to further details. For internal class details, see the relevant section in the module's design document.
