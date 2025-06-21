````markdown
# GLTF Scene Loader Design Document

## 1. Purpose

The GLTF Scene Loader imports 3D scenes and assets from GLTF/GLB files into the engine's scene graph and resource systems. It translates GLTF nodes, meshes, materials, and textures into engine-native representations, registering all resources with the appropriate per-scene systems for efficient rendering and management.

---

## 2. Responsibilities

- Parse GLTF/GLB files and validate their structure.
- Convert GLTF nodes into engine scene nodes (hierarchy, transforms).
- Import meshes, materials, and textures, registering them with the Mesh System, Material System, and other relevant systems for the target scene.
- Use the stateless Resource Allocator for all GPU resource allocation (buffers, images, etc.).
- Track all created resource handles for proper cleanup on scene unload (handled by per-scene systems).
- Report errors and warnings for unsupported or malformed GLTF features.
- (Future) Support loading of animations and skinning data.

---

## 3. Structure

- `src/scene/GLTFSceneLoader.h` / `.cpp` – Main loader implementation.
- `src/scene/Scene.h` / `.cpp` – Scene graph integration.
- `src/assets/` – Asset creation (meshes, textures, materials).
- `third_party/fastgltf/` – GLTF parsing library.

---

## 4. Key Classes & Public API

### GLTFSceneLoader
- `bool loadSceneFromGLTF(const std::string& path, SceneHandle scene)`
  - Loads a GLTF/GLB file and populates the given scene, registering all resources with the appropriate per-scene systems.
- `std::vector<std::string> getLoadWarnings() const`
- `std::vector<std::string> getLoadErrors() const`

### Internal Helpers
- GLTFNodeImporter: Converts GLTF nodes to engine nodes and registers them with the scene.
- GLTFMeshImporter: Imports mesh primitives and attributes, registers them with the Mesh System.
- GLTFMaterialImporter: Imports materials and textures, registers them with the Material System.

---

## 5. Data Flow & Responsibilities

- The loader parses the GLTF file using fastgltf.
- For each GLTF node:
  - Creates a corresponding engine Node and registers it with the scene.
  - Sets up hierarchy and transforms.
- For each mesh/material/texture:
  - Allocates GPU resources via the stateless Resource Allocator.
  - Registers the created resources with the Mesh System, Material System, etc., for the target scene.
- All resource handles are owned and tracked by the per-scene systems. Cleanup is automatic on scene destruction.
- Loader reports any unsupported features or errors for diagnostics.

---

## 6. Ownership and Lifetime

- All resources (meshes, materials, textures) are owned by the scene into which they are loaded.
- No resources are shared between scenes.
- The loader does not own or cache any resources after loading; all ownership is transferred to the appropriate per-scene system.

---

## 7. Summary

- The GLTF Scene Loader is a stateless importer that populates a scene and its resource systems from a GLTF/GLB file.
- All resource allocation is per-scene, with explicit registration to the Mesh, Material, and other systems.
- The loader uses the stateless Resource Allocator for GPU resources.
- No global resource managers or cross-scene sharing.
- All APIs and data flows are explicit, modular, and scene-scoped.

---

This document should be updated as GLTF loading capabilities evolve.
````
