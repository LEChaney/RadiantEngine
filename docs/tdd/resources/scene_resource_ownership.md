# Scene Resource Ownership & Lifetime Design Document

## Purpose

This document describes the overall resource ownership model for the engine. It defines which system owns which resources, how resources are associated with scene nodes, and how resource lifetimes are managed across scene loading/unloading and node removal. The goal is to ensure clear boundaries, robust cleanup, and efficient resource usage.

---

## 1. Ownership Principles

- **Scene Ownership:** All resources (meshes, materials, textures, buffers, etc.) are owned directly by a specific scene. There is no resource sharing between scenes.
- **No Reference Counting:** Resources are not reference-counted or shared between nodes, meshes, or materials. Each scene owns all resources it needs for its nodes.
- **Allocator/Service Pattern:** The Resource Allocator (and Descriptor Allocator) only allocates and deallocates GPU resources on request; it does not track or own higher-level resources.
- **System Responsibility:** Higher-level systems (MeshSystem, MaterialSystem, LightSystem, etc.) are typically global (singleton-like) and manage resources for all loaded scenes. Each resource is explicitly owned by a specific scene, and systems are responsible for releasing a scene's resources when that scene is unloaded. 
- **Scene-Resource Association:** Each system maintains an explicit mapping (such as a map or registry) from Scene* (or scene ID) to the set of resources (meshes, materials, etc.) owned by that scene. This mapping ensures that all resources can be efficiently released when a scene is unloaded, and prevents accidental sharing between scenes.
- **Node Association:** Resources are associated with scene nodes via handles or indices, but the scene graph itself does not own or manage any resources.
- **Explicit Load/Unload:** Resources are loaded only when a scene is loaded, and unloaded only when a scene is explicitly unloaded. Scene switching does not trigger resource loading or unloading.

---

## 2. Resource Ownership Table

| Resource Type      | Owner System      | Allocator Used         | Association         |
|--------------------|------------------|------------------------|---------------------|
| Mesh (CPU+GPU)     | MeshSystem       | ResourceAllocator      | NodeHandle(s)       |
| Material           | MaterialSystem   | ResourceAllocator      | Mesh, NodeHandle(s) |
| Texture            | MaterialSystem   | ResourceAllocator      | Material            |
| Light Data         | LightSystem      | (optional)             | NodeHandle(s)       |
| DrawData           | DrawDataManager  | ResourceAllocator      | NodeHandle(s)       |
| Descriptor Sets    | (various)        | DescriptorAllocator    | System/DrawData     |

---

## 3. Association Model

- **NodeHandle:** All system data (meshes, materials, lights, etc.) is associated to scene nodes via a NodeHandle. Multiple nodes in the same scene may reference the same mesh/material, but there is no sharing between scenes.
- **Registries/Maps:** Systems may maintain registries or maps for quick lookup from NodeHandle to resource data within a scene.
- **No Back-References:** The scene graph does not know what resources are attached to each node; all associations are managed by the systems.

---

## 4. Lifetime Management

- **Scene Load:** When a scene is loaded, all required resources (meshes, materials, textures, etc.) are loaded/created by their respective systems and associated with nodes. All resources are owned by the scene and persist until the scene is explicitly unloaded.
- **Node Removal:** When a node is removed, each system is responsible for cleaning up or detaching any resources/data associated with that node. However, the underlying resources remain alive as long as the scene is loaded.
- **Scene Unload:** When a scene is unloaded, all systems release all resources associated with that scene. This includes deallocating GPU resources via the Resource Allocator.
- **No Resource Sharing Between Scenes:** Each scene has its own set of resources. There is no sharing or reference counting between scenes.

---

## 5. Renderer/System Interaction

- The renderer does not own or manage resource lifetimes. It only consumes data (e.g., MeshDrawData, LightDrawData) provided by the systems.
- Systems must ensure that all resources referenced by draw data are valid for the duration of a frame.
- When a scene switch occurs, the renderer updates its pointers to the new scene's data. Resources for all loaded scenes remain alive until their scene is explicitly unloaded.

---

## 6. Example Flow

1. **Scene Load:**
    - SceneLoader parses a scene file and creates nodes in the scene graph.
    - For each mesh/material/light, the appropriate system loads or references the resource and associates it with the node(s).
    - Systems allocate GPU resources as needed via the Resource Allocator.
2. **During Runtime:**
    - Systems manage their resources and update draw data as needed.
    - Renderer consumes draw data for rendering.
3. **Node Removal:**
    - When a node is removed, each system checks for and cleans up any resources/data associated with that node. Underlying resources remain alive as long as the scene is loaded.
4. **Scene Unload:**
    - All systems release all resources associated with the scene.
    - All GPU resources are deallocated via the Resource Allocator.

---

## 7. Summary

- All resources are owned by a specific scene; no sharing or reference counting between scenes.
- The Resource Allocator is a service, not an owner.
- Scene systems are responsible for associating, tracking, and releasing all resources they use for their scene.
- The renderer is a consumer only.
- Resource loading/unloading only occurs on scene load/unload, not on scene switch.

---

This document should be updated as the resource model evolves.
