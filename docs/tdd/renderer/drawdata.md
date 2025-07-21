# Draw Data Gathering, Culling, and Instance Buffer Management

This document describes an efficient system for gathering draw data and per-instance data from a scene graph, performing view frustum culling, and preparing data for instanced rendering. The approach is designed for modern graphics APIs (Vulkan, D3D12, etc.), supports bindless material systems, and uses two persistently mapped instance buffers (one per frame-in-flight). The instance buffer offset for each draw is communicated to shaders via push constants.

---

## Key Concepts

- **Draws are grouped by (Mesh, MeshSection, Material) key.**
- **Instance data for all draws is flattened into a single buffer per frame.**
- **Each draw records its offset and count into the instance buffer.**
- **Culling is performed during gather, so only visible instances are processed.**
- **Two persistently mapped buffers are used (one per frame-in-flight).**
- **Descriptor set is created once at initialization and always points to the current frame's buffer.**
- **The per-draw instance buffer offset is passed to the shader via push constants.**

---

## Data Structures

```cpp
struct DrawKey {
    Mesh* mesh;
    MeshSection* section;
    Material* material;
    // operator== and hash for use in unordered_map
};

struct DrawData {
    Mesh* mesh;
    MeshSection* section;
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

## Gathering and Culling

1. **Build per-draw lists of instance data during gather:**

```cpp
std::vector<DrawData> draw_data;
std::unordered_map<DrawKey, size_t> draw_key_map;
std::vector<std::vector<InstanceData>> per_draw_instances;

for (const auto& mesh_instance : scene.mesh_instances) {
    for (auto* section : mesh_instance.mesh->sections) {
        auto* material_instance = mesh_instance.get_material_instance_for_section(section);
        DrawKey key{mesh_instance.mesh, section, material_instance->GetMaterial()};

        size_t draw_idx;
        auto it = draw_key_map.find(key);
        if (it == draw_key_map.end()) {
            DrawData draw;
            draw.mesh = mesh_instance.mesh;
            draw.section = section;
            draw.material = material_instance->GetMaterial();
            draw.pipeline = draw.material->pipeline;
            draw.instance_offset = 0; // Will be set later
            draw.instance_count = 0;
            draw_data.push_back(draw);
            draw_idx = draw_data.size() - 1;
            draw_key_map[key] = draw_idx;
        } else {
            draw_idx = it->second;
        }

        Matrix4x4 bounds_to_world = mesh_instance.transform * section->bounds_to_mesh;
        if (!is_visible(camera, bounds_to_world)) continue;

        per_draw_instances[draw_idx].push_back({
            mesh_instance.transform,
            material_instance->GetParameterBufferAddress()
        });
    }
}
```

2. **Flatten per-draw instance data into a single buffer and assign offsets/counts:**

```cpp
std::vector<InstanceData> instance_data;
uint32_t running_offset = 0;
for (size_t draw_idx = 0; draw_idx < draw_data.size(); ++draw_idx) {
    auto& draw = draw_data[draw_idx];
    const auto& instances = per_draw_instances[draw_idx];
    draw.instance_offset = running_offset;
    draw.instance_count = static_cast<uint32_t>(instances.size());
    instance_data.insert(instance_data.end(), instances.begin(), instances.end());
    running_offset += draw.instance_count;
}
```

---

## Instance Buffer Management

- **Allocate two large, persistently mapped buffers at initialization** (one for each frame-in-flight).
- **Each frame, write all visible instance data to the mapped pointer for the current frame.**
- **No map/unmap required.**
- **Descriptor set is created once and always points to the current frame's buffer.**

```cpp
constexpr uint32_t NUM_FRAMES_IN_FLIGHT = 2;
std::array<Buffer, NUM_FRAMES_IN_FLIGHT> instanceBuffers;
std::array<void*, NUM_FRAMES_IN_FLIGHT> mappedPtrs;

for (uint32_t i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i) {
    instanceBuffers[i] = CreateBuffer(
        MAX_INSTANCE_COUNT * sizeof(InstanceData),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    mappedPtrs[i] = instanceBuffers[i].map(); // Map once, keep mapped
}

// Each frame:
uint32_t frameIdx = GetCurrentFrameIndex(); // 0 or 1
memcpy(mappedPtrs[frameIdx], instance_data.data(), instance_data.size() * sizeof(InstanceData));
```

---

## Descriptor Set Binding and Drawing

- **Descriptor set is created at initialization and always points to the current frame's buffer.**
- **No need to update the descriptor set each frame.**
- **For each draw, push the instance offset as a push constant.**
- **Draw using instancing.**

```cpp
for (const auto& draw : draw_data) {
    if (draw.instance_count == 0) continue;

    BindPipeline(draw.pipeline);
    BindDescriptorSet(globalDescriptorSet);
    BindDescriptorSet(instanceDescriptorSet, instanceBuffers[frameIdx]);

    // Push instance offset to the shader
    PushConstants pc;
    pc.instance_offset = draw.instance_offset;
    SetPushConstants(pc);

    DrawInstanced(draw.mesh, draw.section, draw.instance_count);
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

## Summary

- **All visible instance data is tightly packed in a single buffer per frame.**
- **Two persistently mapped buffers are used (one per frame-in-flight).**
- **Descriptor set is created once and always points to the current frame's buffer.**
- **No map/unmap or descriptor set updates required per frame.**
- **Each draw uses a push constant to specify its instance data offset.**
- **This approach is efficient, simple, and scales well for modern rendering engines.**
