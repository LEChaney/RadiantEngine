# Light System Design Document

## Purpose

The Light System manages all light data within a scene. It provides APIs for adding, removing, and updating lights, and maintains the association between lights and scene nodes. The system is designed for clarity, modularity, and high performance, with explicit ownership and lifetime rules.

## Ownership and Lifetime

- **Per-Scene Ownership:** All lights are owned by the scene that created them. There is no sharing of light data between scenes.
- **Lifetime:** Lights exist for the lifetime of their owning scene. When a scene is destroyed, all its lights are destroyed.
- **No Global State:** The Light System does not own or cache lights globally. All state is per-scene.

## Light Allocation and Management

- **Light Creation:** Lights are created via the Light System API, which takes a scene handle, node handle, and light description. The system allocates and tracks the light within the scene.
- **Light Destruction:** Lights are explicitly destroyed via the API, or implicitly when the scene is destroyed.
- **Light Lookup:** Lights are referenced by opaque handles or indices, which are only valid within the owning scene.

## Association with Scene Nodes

- **Node Association:** Each light is associated with a scene node via its node handle. The Light System maintains the mapping from node handles to light indices.
- **Transform Synchronization:** The Light System does not store world transforms. After scene graph updates, transforms must be synchronized explicitly by the system user.

## Stateless Resource Allocation

- The Light System uses the Resource Allocator for any GPU resource allocation (e.g., light buffers). The allocator is stateless and non-owning.
- The Light System is responsible for tracking which GPU resources belong to which light, and for releasing them when the light is destroyed.

## API Overview

- `LightHandle addLight(SceneHandle scene, NodeHandle node, const LightDesc& desc);`
- `void removeLight(SceneHandle scene, LightHandle handle);`
- `void updateLight(SceneHandle scene, LightHandle handle, const LightDesc& desc);`
- `const Light& getLight(SceneHandle scene, LightHandle handle);`
- `std::vector<LightHandle> getLightsForNode(SceneHandle scene, NodeHandle node);`

## Renderer Interface

- The renderer queries the Light System for light data when building lighting passes.
- All light data is provided in a flat, cache-friendly format for efficient GPU upload.
- The Light System does not perform any rendering; it only manages data and associations.

## Summary

- Lights are owned per scene, never shared.
- The Light System is modular, stateless (except for per-scene state), and interacts with the Resource Allocator for GPU resources.
- Lights are associated with scene nodes by handle; the Light System maintains this mapping.
- All APIs are explicit and scene-scoped for clarity and safety.
