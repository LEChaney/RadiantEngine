# DrawDataManager: Draw Data Gathering, Culling, and Instance Buffer Management

This document describes the design and responsibilities of the `DrawDataManager` class, which efficiently gathers draw and per-instance data from a scene graph, performs view frustum culling, and prepares data for instanced rendering. The system is designed for modern graphics APIs (Vulkan, D3D12, etc.), supports bindless material systems, and uses two persistently mapped instance buffers (one per frame-in-flight). Instance buffer offsets for each draw are communicated to shaders via push constants.

---

## Overview

The `DrawDataManager` class is responsible for:

- Grouping draws by (Mesh, MeshSection, Material) key.
- Gathering and culling per-instance data.
- Flattening all visible instance data into a single buffer per frame.
- Managing two persistently mapped instance buffers (double buffering).
- Providing per-draw instance buffer offsets and counts.
- Handling descriptor set binding and push constant setup for rendering.

---

## Data Structures

```cpp
struct DrawKey {
    Mesh* mesh;
    MeshSection* mesh_section;
    Material* material;
    // operator== and hash for use in unordered_map
};

struct DrawData {
    Mesh* mesh;
    MeshSection* mesh_section;
    Material* material;
    Pipeline* pipeline;
    uint32_t instance_offset; // Offset into the instance buffer
    uint32_t instance_count;  // Number of instances for this draw
};

struct InstanceData {
    Matrix4x4 transform;
    VkDeviceAddress material_address; // For bindless material params
};
```

---

## Class Responsibilities and API

```cpp
class DrawDataManager {
public:
    DrawDataManager(uint32_t max_instances, uint32_t num_frames_in_flight);

    // Gathers and culls draw and instance data for the current frame
    void gather_and_cull(const Scene& scene, const Camera& camera);

    // Uploads instance data to the current frame's buffer
    void upload_instance_data(uint32_t frame_idx);

    // Returns the list of draws for the current frame
    const std::vector<DrawData>& get_draw_data() const;

    // Returns the instance buffer for the current frame
    Buffer& get_instance_buffer(uint32_t frame_idx);

    // Returns the descriptor set for the instance buffer for the current frame
    DescriptorSet get_instance_descriptor_set(uint32_t frame_idx) const;

private:
    uint32_t max_instances_;
    uint32_t num_frames_in_flight_;

    std::vector<DrawData> draw_data_;
    std::vector<InstanceData> instance_data_;
    std::unordered_map<DrawKey, size_t> draw_key_map_;
    std::vector<std::vector<InstanceData>> per_draw_instances_;

    std::array<Buffer, 2> instance_buffers_;
    std::array<void*, 2> mapped_ptrs_;
    std::array<DescriptorSet, 2> instance_descriptor_sets_;
};
```

---

## Usage Pattern

### 1. **Initialization**

- Allocate two large, persistently mapped buffers (one per frame-in-flight).
- Create a descriptor set pointing to the instance buffer (updated only if buffer handles change).

### 2. **Gathering and Culling**

- For each mesh instance and section, group by `(Mesh, MeshSection, Material)`.
- Cull using the camera frustum.
- Store visible instances in per-draw lists.

### 3. **Flattening and Offset Assignment**

- Concatenate all per-draw instance lists into a single flat buffer.
- Assign each draw its offset and count into the buffer.

### 4. **Uploading Instance Data**

- Each frame, write all visible instance data to the mapped pointer for the current frame's buffer.

### 5. **Rendering**

- For each draw:
    - Bind the pipeline and descriptor sets.
    - Push the instance offset as a push constant.
    - Issue the instanced draw call.

---

## Example Usage

```cpp
// At frame start:
draw_data_manager.gather_and_cull(scene, camera);
draw_data_manager.upload_instance_data(frame_idx);

const auto& draw_list = draw_data_manager.get_draw_data();
DescriptorSet instance_set = draw_data_manager.get_instance_descriptor_set(frame_idx);

for (const auto& draw : draw_list) {
    if (draw.instance_count == 0) continue;

    bind_pipeline(draw.pipeline);
    bind_descriptor_set(global_descriptor_set);
    bind_descriptor_set(instance_set);

    PushConstants push_constants;
    push_constants.instance_offset = draw.instance_offset;
    set_push_constants(push_constants);

    draw_instanced(draw.mesh, draw.mesh_section, draw.instance_count);
}
```

---

## Shader Usage Example (GLSL)

```glsl
layout(set = 2, binding = 0) readonly buffer InstanceBuffer {
    InstanceData instances[];
};

layout(push_constant) uniform PushConstants {
    uint instance_offset;
};

void main() {
    uint idx = gl_InstanceIndex + instance_offset;
    InstanceData inst = instances[idx];
    // Use inst.transform and inst.material_address
}
```

---

## Example Implementation

Below is a sample implementation of the core gather, cull, and flatten logic for `DrawDataManager`:

```cpp
void DrawDataManager::gather_and_cull(const Scene& scene, const Camera& camera) {
    draw_data_.clear();
    instance_data_.clear();
    draw_key_map_.clear();
    per_draw_instances_.clear();

    for (const auto& mesh_instance : scene.mesh_instances) {
        for (auto* mesh_section : mesh_instance.mesh->sections) {
            auto* material_instance = mesh_instance.get_material_instance_for_section(mesh_section);
            DrawKey key{mesh_instance.mesh, mesh_section, material_instance->GetMaterial()};

            size_t draw_idx;
            auto it = draw_key_map_.find(key);
            if (it == draw_key_map_.end()) {
                DrawData draw;
                draw.mesh = mesh_instance.mesh;
                draw.mesh_section = mesh_section;
                draw.material = material_instance->GetMaterial();
                draw.pipeline = draw.material->pipeline;
                draw.instance_offset = 0; // Will be set later
                draw.instance_count = 0;
                draw_data_.push_back(draw);
                draw_idx = draw_data_.size() - 1;
                draw_key_map_[key] = draw_idx;
            } else {
                draw_idx = it->second;
            }

            Matrix4x4 bounds_to_world = mesh_instance.transform * mesh_section->bounds_to_mesh;
            if (!is_visible(camera, bounds_to_world)) continue;

            per_draw_instances_[draw_idx].push_back({
                mesh_instance.transform,
                material_instance->GetParameterBufferAddress()
            });
        }
    }

    // Flatten per-draw instance data into a single buffer and assign offsets/counts
    uint32_t running_offset = 0;
    for (size_t draw_idx = 0; draw_idx < draw_data_.size(); ++draw_idx) {
        auto& draw = draw_data_[draw_idx];
        const auto& instances = per_draw_instances_[draw_idx];
        draw.instance_offset = running_offset;
        draw.instance_count = static_cast<uint32_t>(instances.size());
        instance_data_.insert(instance_data_.end(), instances.begin(), instances.end());
        running_offset += draw.instance_count;
    }
}

void DrawDataManager::upload_instance_data(uint32_t frame_idx) {
    memcpy(mapped_ptrs_[frame_idx], instance_data_.data(), instance_data_.size() * sizeof(InstanceData));
}
```

---

## Summary

- `DrawDataManager` centralizes draw and instance data management for efficient instanced rendering.
- Uses two persistently mapped buffers for per-frame-in-flight data.
- Descriptor set is created once and always points to the current frame's buffer.
- No map/unmap or descriptor set updates required per frame.
- Each draw uses a push constant to specify its instance data offset.
- This approach is efficient, simple, and scales well for modern rendering engines.
