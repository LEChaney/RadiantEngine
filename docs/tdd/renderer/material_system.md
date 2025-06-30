# Material System Design Document

## Summary Table

| Responsibility                | Owned/Managed by MaterialSystem | Delegated To                |
|-------------------------------|:-------------------------------:|-----------------------------|
| Material definitions          | Yes                             |                             |
| Material descriptor sets      | Yes                             |                             |
| Material parameter buffers    | Yes                             |                             |
| Texture resources             | No                              | TextureManager              |
| Pipeline/layout references    | No                              | PipelineManager             |
| Per-instance assignment       | No                              | MeshSystem                  |
| Global descriptor sets        | No                              | Renderer                    |

## Purpose

The MaterialSystem is responsible for managing all material definitions within a scene, including their associated parameter buffers and descriptor sets for GPU binding. It provides APIs for creating, updating, and querying materials, and handles the allocation and update of material-specific GPU resources. The MaterialSystem coordinates with the TextureManager for texture resources and with the PipelineManager for pipeline and layout references, enabling efficient sharing of resources and supporting flexible material workflows.

---

## 1. Responsibilities

- Manage all material definitions, descriptor sets, and parameter buffers per scene.
- Provide APIs for creating, destroying, and querying materials.
- Integrate with the Resource Allocator for GPU resource management (buffers).
- Integrate with the TextureManager for all texture resource requests, deduplication, and lifetime management.
- Provide Vulkan pipeline/layout/descriptor set(s) for each material, referencing shared resources from the TextureManager and PipelineManager. The MaterialSystem creates and updates descriptor sets and parameter buffers as needed.
- Support per-mesh-instance material assignment: each mesh instance specifies a set of materials (one per mesh section) to use for rendering.
- Notify the DrawDataManager when material resources change, so draw data can be updated accordingly. The MeshSystem is responsible for notifying the DrawDataManager when per-instance material assignments change.
- Track all material allocations per scene for efficient release on scene unload.

---

## 2. Material Association and Data Flow Model

### 2.1 Per-Instance Material Assignment

- Materials are not assigned to meshes directly. Instead, each mesh instance (i.e., a mesh attached to a node) specifies a set of materials (one per mesh section) to use for rendering that instance. This enables the same mesh to be drawn with different materials in different parts of the scene, supporting instancing and material overrides.
- The MeshSystem is responsible for storing and managing the per-instance material set, and for providing this information to the DrawDataManager when generating draw data.

### 2.2 Texture Resource Management

- Texture resources referenced by materials are not owned by the Material System. Instead, the Material System requests textures from the TextureManager (by file path, asset ID, or other identifier) when creating or updating a material, and stores the returned `TextureHandle` in the material data.
- The TextureManager ensures that each unique texture is loaded only once per scene (or globally, if configured), and manages the lifetime and deduplication of all texture GPU resources.

### 2.3 Pipeline and Pipeline Layout Sharing

- Pipelines and pipeline layouts are managed by the PipelineManager and are shared among all materials that use the same shader code and layout.
- When a material is created, the MaterialSystem determines the required pipeline/layout (based on shader, defines, render pass, etc.) and requests it from the PipelineManager.
- The MaterialSystem stores only a handle to the pipeline and layout, enabling efficient sharing of expensive pipeline resources and supporting many materials with minimal overhead.
- Descriptor sets, textures, and material parameters remain unique per material, while pipelines/layouts are shared.

### 2.4 MaterialDrawData Separation

- The Material System distinguishes between high-level, editable `Material` objects (which may reference textures, parameters, etc.) and the low-level, immutable `MaterialDrawData` struct used for rendering.
- `MaterialDrawData` contains only the pipeline, pipeline layout, and descriptor set required for a draw call. It does not reference textures or material-specific buffers directly; these are bound via the descriptor set.
- This separation allows the Material System to manage editable material data for tools and asset workflows, while the renderer and DrawDataManager only interact with the minimal, cache-friendly `MaterialDrawData` for each draw.
- When a material is created or updated, the Material System generates or updates the corresponding `MaterialDrawData` and notifies the DrawDataManager to update draw data as needed.

### 2.5 Global (Shared) Descriptor Sets and Renderer Coordination

- Some pipelines and pipeline layouts require a global (shared) descriptor set, typically bound at set 0, for data such as camera parameters, light data, and other per-frame or per-scene resources.
- The MaterialSystem is responsible for material-specific descriptor sets (e.g., set 1), while the renderer is responsible for creating, updating, and binding global descriptor sets (set 0) each frame.
- The PipelineManager exposes metadata about required descriptor set layouts for each pipeline layout, allowing the renderer to query which sets must be bound for a given draw.
- During rendering, the renderer binds the global descriptor set(s) at the appropriate set index (e.g., set 0), followed by the material descriptor set(s) (e.g., set 1), before issuing draw calls.
- This convention ensures that all required data is available to the shaders, and that global and material data are managed by the appropriate systems.

---

## 3. Data Structures

- `Material` struct: Contains all data needed for a material, including shader references, parameter buffers, texture handles (from the TextureManager), Vulkan pipeline/layout/descriptor set(s), and any additional buffers required for material parameters or dynamic data. The MaterialSystem is responsible for creating, updating, and releasing these buffers as material resources change.
- `MaterialDrawData` struct: Contains only the pipeline, pipeline layout, and descriptor set required for a draw call. This struct is generated by the Material System and stored inline in `MeshDrawData` for each draw call.
- `MaterialHandle`: Opaque handle or index for referencing materials.
- Per-scene material registry: Maps handles to material data for each scene.

### Descriptor Structs

- `MaterialDesc`: Describes all parameters required to create a material. Fields typically include:
  - Shader references (vertex, fragment, etc.)
  - Material parameter values (floats, vectors, etc.)
  - Texture references or file paths
  - Render state overrides (optional)
  - Pipeline and layout requirements (may reference PipelineDesc/PipelineLayoutDesc)
  - Any user data or metadata for editor integration
- `MaterialResourceUpdate`: Used to update an existing material's parameters or textures. Fields typically include:
  - Updated parameter values
  - Updated texture references
  - Optional: flags indicating which resources changed

These descriptor structs are used for material creation, updates, and as the basis for generating the minimal MaterialDrawData struct for rendering.

---

## 4. API Overview

- `MaterialHandle createMaterial(Scene* scene, const MaterialDesc& desc);` // Creates a new material for a scene, requesting all required textures from the TextureManager
- `void destroyMaterial(Scene* scene, MaterialHandle handle);` // Destroys a material and releases its resources, including releasing texture handles via the TextureManager
- `const Material& getMaterial(Scene* scene, MaterialHandle handle) const;` // Retrieves material data
- `VkPipeline getPipeline(MaterialHandle handle) const;` // Returns the Vulkan pipeline for a material
- `VkPipelineLayout getPipelineLayout(MaterialHandle handle) const;`
- `VkDescriptorSet getDescriptorSet(MaterialHandle handle) const;`
- `void updateMaterialResources(Scene* scene, MaterialHandle handle, const MaterialResourceUpdate& update);` // Updates textures, parameters, etc., requesting new textures from the TextureManager as needed

---

## 5. Integration with Other Systems

- **MeshSystem:**
  - Mesh instances specify a set of material handles (one per mesh section) to use for rendering.
  - The MeshSystem provides the per-instance material set to the DrawDataManager when generating draw data.
- **DrawDataManager:**
  - When generating or updating draw data, queries the MeshSystem for the material handle for each mesh section in an instance, then queries the MaterialSystem for the Vulkan pipeline, layout, and descriptor set for that material.
  - Updates the relevant fields in `MeshDrawData`.
- **TextureManager:**
  - The Material System requests all required textures from the TextureManager when creating or updating materials, and stores only texture handles in material data. It is responsible for creating and updating the Vulkan descriptor set(s) for each material, using the GPU handles provided by the TextureManager.
  - The TextureManager deduplicates textures, provides GPU handles (VkImage, VkImageView, VkSampler) for descriptor set creation, and manages texture lifetime and cleanup.
  - When a material is destroyed, the Material System releases its texture handles via the TextureManager, which manages reference counting or per-scene ownership and releases GPU resources when no longer needed.
- **Resource Allocator:**
  - All GPU resources (buffers, including any additional constant or dynamic buffers) for materials are allocated and released via the Resource Allocator. Texture GPU resources are managed by the TextureManager.
- **Renderer:**
  - Uses the Vulkan data in `MeshDrawData` for draw call submission; does not interact with high-level material or texture handles.

---

## 6. Ownership and Lifetime

- All materials and their GPU resources are owned per scene.
- Texture resources are owned and deduplicated by the TextureManager, and may be shared across multiple materials.
- No cross-scene sharing or reference counting for materials; texture sharing is managed by the TextureManager.
- Materials are destroyed and resources released when a scene is unloaded. Texture resources are released by the TextureManager when no longer referenced by any material in the scene.

---

## 7. Notes

- The MaterialSystem does not track mesh instance usage; it only manages material resources and provides APIs for lookup and update.
- The MeshSystem is responsible for per-instance material assignment and for updating draw data when assignments change.
- The TextureManager is responsible for deduplication, GPU resource management, and lifetime of all textures referenced by materials.
- This design supports instancing, material overrides, efficient resource management, and safe texture sharing.

---

## 8. Example Usage

```cpp
// Create a new material for a scene
MaterialHandle matHandle = materialSystem.createMaterial(scene, matDesc);

// Update material resources (e.g., change textures or parameters)
materialSystem.updateMaterialResources(scene, matHandle, updateDesc);

// Retrieve the minimal draw data for rendering
const MaterialDrawData& drawData = materialSystem.getMaterial(scene, matHandle).drawData;

// Destroy a material when no longer needed
materialSystem.destroyMaterial(scene, matHandle);
```

---

This document should be updated as the material system and texture manager evolve.
