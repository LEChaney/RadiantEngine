# Material Shader Parameter Layout Sketch (Bindless)

This sketch demonstrates a typical GLSL layout for a bindless material system, including optional material parameter collections.

---

```glsl
// Global data (e.g. camera parameters)
layout(set = 0, binding = 0) uniform CameraParams {
    mat4 view;
    mat4 proj;
    vec3 cameraPosition;
    // ...other global parameters...
} camera;

// Bindless texture array
layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[];

// Instance data buffer (SSBO)
layout(set = 2, binding = 0) readonly buffer InstanceBuffer {
    InstanceData instances[];
};

// Optional: buffer reference for shared parameter collection (e.g., lighting)
layout(buffer_reference, std430) buffer LightingParams {
    vec3 lightDirection;
    vec4 ambientColor;
    // ...other shared parameters...
};

// Buffer reference for per-instance material parameters
layout(buffer_reference, std430) buffer MaterialParams {
    vec4 baseColor;
    float roughness;
    float metallic;
    // ...other material parameters...
    LightingParams lighting; // Optional, may be null
    uint textureIndices[4]; // Indices into bindlessTextures
};

// Per-instance data structure (matches InstanceBuffer layout)
struct InstanceData {
    mat4 model;
    MaterialParams material;
};

// Main shader entry
void main() {
    uint instanceIndex = ...; // e.g., gl_InstanceIndex or vertex attribute
    InstanceData instance = instances[instanceIndex];

    // Access global camera params
    mat4 view = camera.view;
    mat4 proj = camera.proj;
    vec3 camPos = camera.cameraPosition;

    // Access material parameters
    vec4 color = instance.material.baseColor;
    float rough = instance.material.roughness;
    float metal = instance.material.metallic;

    // Access textures via bindless index
    uint albedoIdx = instance.material.textureIndices[0];
    vec4 albedo = texture(bindlessTextures[albedoIdx], ...);

    // Access shared lighting parameters if present
    if (instance.material.lighting != LightingParams(0)) {
        vec3 lightDir = instance.material.lighting.lightDirection;
        vec4 ambient = instance.material.lighting.ambientColor;
        // ...use lighting params...
    }

    // ...rest of shading code...
}
```

---

## Notes
- `MaterialParams` is a buffer reference, passed per-instance (via SSBO or push constant, or fetched from a device address array).
- `LightingParams` is an optional buffer reference, can be null or omitted if not used.
- Texture indices are stored in the material struct and used to index into the bindless texture array.
- This layout supports flexible, validated, and scalable material management for both rasterization and ray tracing pipelines.

---

## CPU-Side Descriptor Set Update Example (Vulkan)

Below is a C++ code sketch for updating descriptor sets to match the above shader layout. This example assumes you have created Vulkan buffers and images for your resources.

```cpp
// Assume VkDevice device, VkDescriptorSet descriptorSet, and resource handles are valid

// Bindless texture array (set=1, binding=0)
VkDescriptorImageInfo textureInfos[MAX_TEXTURES];
for (uint32_t i = 0; i < textureCount; ++i) {
    textureInfos[i].imageView = textures[i].view;
    textureInfos[i].sampler = textures[i].sampler;
    textureInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}
VkWriteDescriptorSet textureWrite{};
textureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
textureWrite.dstSet = descriptorSet;
textureWrite.dstBinding = 0;
textureWrite.dstArrayElement = 0;
textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
textureWrite.descriptorCount = textureCount;
textureWrite.pImageInfo = textureInfos;

// Global camera params (set=0, binding=0)
VkDescriptorBufferInfo cameraBufferInfo{};
cameraBufferInfo.buffer = cameraBuffer;
cameraBufferInfo.offset = 0;
cameraBufferInfo.range = sizeof(CameraParams);
VkWriteDescriptorSet cameraWrite{};
cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
cameraWrite.dstSet = descriptorSet;
cameraWrite.dstBinding = 0;
cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
cameraWrite.descriptorCount = 1;
cameraWrite.pBufferInfo = &cameraBufferInfo;

// Instance data buffer (set=2, binding=0)
VkDescriptorBufferInfo instanceBufferInfo{};
instanceBufferInfo.buffer = instanceBuffer;
instanceBufferInfo.offset = 0;
instanceBufferInfo.range = VK_WHOLE_SIZE;
VkWriteDescriptorSet instanceWrite{};
instanceWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
instanceWrite.dstSet = descriptorSet;
instanceWrite.dstBinding = 2;
instanceWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
instanceWrite.descriptorCount = 1;
instanceWrite.pBufferInfo = &instanceBufferInfo;

// Write all at once
std::array<VkWriteDescriptorSet, 3> writes = { textureWrite, cameraWrite, instanceWrite };
vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
```

### Notes
- For buffer references (e.g., `MaterialParams`, `LightingParams`), you typically store device addresses in your instance buffer and fetch in the shader. No direct descriptor update is needed for buffer references, only for the containing SSBO/UBO.
- Descriptor set layout must match the shader bindings and types.
- For bindless textures, use an array of `COMBINED_IMAGE_SAMPLER` descriptors.
- For advanced usage (e.g., ray tracing), you may use `VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR` and device address features.

---

## Command Buffer Descriptor Set Binding Example (Vulkan)

This is an example of how to bind the descriptor sets in a command buffer for drawing, matching the shader and descriptor set layout.

```cpp
// Assume VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, and descriptor sets are valid

vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
    0, 1, &globalSet, 0, nullptr);    // set 0: global
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
    1, 1, &textureSet, 0, nullptr);   // set 1: textures
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
    2, 1, &instanceSet, 0, nullptr);  // set 2: instances

// ...bind pipeline, vertex buffers, index buffer, etc.

vkCmdDrawIndexed(cmd, indexCount, instanceCount, 0, 0, 0);
```

### Notes
- Descriptor sets must be bound in the correct order matching the pipeline layout.
- Ensure that the pipeline is compatible with the bound descriptor sets (e.g., same layout, bindings).
- This example assumes indexed drawing; adjust for non-indexed or instanced drawing as needed.

