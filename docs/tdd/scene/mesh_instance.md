# Mesh Instance Design Document

## Purpose

Describes the representation and management of mesh instances within a scene. Mesh instances reference mesh resources and provide per-instance scene node attachment, material, and visibility data.

---

## Responsibilities

- Represent a single occurrence of a mesh in the scene
- Store per-instance data: transform, material assignment, visibility, etc.
- Reference mesh resources via handles
- Integrate with scene graph and rendering systems

---

## Mesh Instance Structure

```cpp
struct MeshInstance {
    MeshHandle mesh;                // Reference to mesh resource
    MaterialHandle material;        // Reference to material
    SceneNodeKey scene_node;        // Reference to the scene node for transform
    bool visible;                   // Visibility flag
    // (Optional) instance-specific data (e.g., selection, animation state)
};
```

---

## API and Usage

- Mesh instances are created by the scene system when adding objects
- Instances reference meshes and materials managed by registries
- Transform and visibility are updated per-frame as needed
- Instances are submitted to the renderer for drawing

---

## Ownership & Lifetime

- Mesh instances are owned by the scene
- Destroyed when the scene or owning object is destroyed

---

## Notes

- Mesh instances decouple resource storage from scene representation
- Multiple instances can reference the same mesh and material
- This document should be updated as instance features evolve
