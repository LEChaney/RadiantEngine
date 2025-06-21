# Material System Design Document

## Purpose

The Material System is responsible for managing all material resources within a scene. It provides APIs for material creation, destruction, and lookup, and maintains the association between materials, meshes, and scene nodes. The system is designed for clarity, modularity, and high performance, with explicit ownership and lifetime rules.

## Ownership and Lifetime

- **Per-Scene Ownership:** All materials are owned by the scene that created them. There is no sharing of material resources between scenes.
- **Lifetime:** Materials exist for the lifetime of their owning scene. When a scene is destroyed, all its materials are destroyed.
- **No Global State:** The Material System does not own or cache materials globally. All state is per-scene.

## Material Allocation and Management

- **Material Creation:** Materials are created via the Material System API, which takes a scene handle and material description. The system allocates and tracks the material within the scene.
- **Material Destruction:** Materials are explicitly destroyed via the API, or implicitly when the scene is destroyed.
- **Material Lookup:** Materials are referenced by opaque handles or indices, which are only valid within the owning scene.

## Association with Meshes and Scene Nodes

- **Mesh Association:** Each mesh draw (see Mesh System) references a material by handle or index. The mapping is maintained by the Mesh System, not the Material System.
- **Node Association:** Materials are not directly associated with scene nodes. Instead, the mesh draw for a node references the material.

## Stateless Resource Allocation

- The Material System uses the Resource Allocator for GPU resource allocation (e.g., textures, uniform buffers). The allocator is stateless and non-owning.
- The Material System is responsible for tracking which GPU resources belong to which material, and for releasing them when the material is destroyed.

## API Overview

- `MaterialHandle createMaterial(SceneHandle scene, const MaterialDesc& desc);`
- `void destroyMaterial(SceneHandle scene, MaterialHandle handle);`
- `const Material& getMaterial(SceneHandle scene, MaterialHandle handle);`
- `MaterialHandle getDefaultMaterial(SceneHandle scene);`

## Renderer Interface

- The renderer queries the Material System for material data when building draw calls.
- All material data is provided in a flat, cache-friendly format for efficient GPU upload.
- The Material System does not perform any rendering; it only manages data and associations.

## Summary

- Materials are owned per scene, never shared.
- The Material System is modular, stateless (except for per-scene state), and interacts with the Resource Allocator for GPU resources.
- Meshes reference materials by handle; the Material System does not track mesh associations.
- All APIs are explicit and scene-scoped for clarity and safety.
