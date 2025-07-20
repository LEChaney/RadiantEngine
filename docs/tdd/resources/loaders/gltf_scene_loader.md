````markdown

# GLTF Scene Loader Design Document

## Purpose

Imports 3D scenes and assets from GLTF/GLB files, creating engine-native resources and scene nodes. Handles asset import and scene graph instancing.

---

## Responsibilities

- Parse GLTF/GLB files and validate structure
- Import meshes, materials, textures as engine resources
- Create scene nodes and mesh instances
- Use Resource Allocator for GPU resources
- Report errors/warnings for unsupported features

---

## Structure & Internals

- `GLTFSceneLoader` class: Main loader
- Uses third-party GLTF parser (e.g., fastgltf)
- Calls MeshManager, MaterialRegistry, TextureManager to create resources
- Instantiates scene nodes and mesh instances

---

## Example API

```cpp
bool load_scene_from_gltf(const std::string& path, SceneHandle scene);
std::vector<std::string> get_load_warnings() const;
std::vector<std::string> get_load_errors() const;
```

---

## Ownership & Lifetime

- All resources are owned per scene by their respective managers
- Loader does not own resources after import

---

## Notes

- Loader is stateless; all resource ownership is transferred to scene systems
- No cross-scene sharing
- This document should be updated as GLTF loading evolves
````
