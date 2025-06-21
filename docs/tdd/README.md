# Technical Design Documents

This folder contains technical design documentation for each major module in the renderer project. Each module has its own subfolder containing a high-level design document (`scene.md`, `renderer.md`, etc.) and, where appropriate, additional detailed documentation for internal classes and subsystems.

## High-Level Design

- [High-Level Design Document](high_level_design.md)

## Structure

- [`renderer/`](renderer/renderer.md) — [Renderer module design and internal documentation](renderer/renderer.md)
    - [FrustumCuller](renderer/renderer.md#frustumculler)
    - [ResourceManager](renderer/renderer.md#resourcemanager)
    - [PipelineManager](renderer/renderer.md#pipelinemanager)
    - [DescriptorManager](renderer/renderer.md#descriptormanager)
    - [VulkanContext](renderer/renderer.md#vulkancontext)
    - [SwapchainManager](renderer/renderer.md#swapchainmanager)
- [`camera/`](camera/camera.md) — [Camera module design and internal documentation](camera/camera.md)
- [`assets/`](assets/gltf_scene_loader.md) — [GLTF Scene Loader Design](assets/gltf_scene_loader.md)
- (Add more as needed: `scene/`, etc.)

Refer to each module's main design document (e.g., [`renderer.md`](renderer/renderer.md)) for an overview and links to further details. For internal class details, see the relevant section in the module's design document.
