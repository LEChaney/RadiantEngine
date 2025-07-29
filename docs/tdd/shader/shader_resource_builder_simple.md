# Simple Shader Resource Builder API Sketch

This document presents a highly condensed, reflection-driven C++ API for setting up all shader resources (buffers, textures, arrays, parameters) in a single place, with a single `Finalize()` call to upload all data and create all descriptor sets. This is ideal for simple or static pipelines where descriptor sets do not need to be swapped per draw.

---

## Example GLSL Shader

```glsl
layout(set = 0, binding = 0) uniform CameraParams {
    mat4 view;
    mat4 proj;
    vec3 cameraPosition;
} camera;

layout(set = 1, binding = 0) uniform sampler2D bindlessTextures[];

layout(set = 2, binding = 0) readonly buffer InstanceBuffer {
    InstanceData instances[];
    ConfigData config;
};

struct InstanceData {
    mat4 model;
    MaterialParams material;
};

struct MaterialParams {
    vec4 baseColor;
    float roughness;
    float metallic;
    uint textureIndices[4];
};

struct ConfigData {
    int foo;
};
```

---

## Ultra-Condensed C++ API Example

```cpp
// 1. Create a single ShaderResourceBuilder for the shader
ShaderResourceBuilder builder(reflection, device);

// 2. Set all resources and parameter values directly
builder.Set("camera.view", cameraViewMatrix);
builder.Set("camera.cameraPosition", cameraPosition);

builder.SetTexture("bindlessTextures", 0, albedoView, albedoSampler);
builder.SetTexture("bindlessTextures", 1, normalView, normalSampler);
builder.SetTexture("bindlessTextures", 2, ormView, ormSampler);

builder.Set("instances[0].model", obj0ModelMatrix);
builder.Set("instances[0].material.baseColor", obj0BaseColor);
builder.Set("instances[0].material.textureIndices[0]", 0); // albedo
builder.Set("instances[0].material.textureIndices[1]", 1); // normal
builder.Set("instances[0].material.textureIndices[2]", 2); // orm
// OR
// Ergonomic array element access for instances
auto instances = builder.Array("instances");
instances.Resize(numInstances); // Explicitly size the array first
for (size_t i = 0; i < numInstances; ++i) {
    auto inst = instances[i];
    inst.Set("model", instanceModels[i]);
    auto mat = inst.Struct("material");
    mat.Set("baseColor", baseColors[i]);
    mat.Set("roughness", roughnesses[i]);
    mat.Set("metallic", metallics[i]);
    // Set texture indices for this material
    for (uint32_t t = 0; t < 3; ++t)
        mat.SetArrayElement("textureIndices", t, textureIndices[i][t]);
}

builder.Set("config.foo", 42);

// ...repeat for other instances/materials as needed...

// 3. Finalize: allocates buffers, uploads data, creates all descriptor sets
builder.Finalize();

// 4. In your draw loop, just bind all sets at once
builder.BindAll(cmd, pipelineLayout);
vkCmdDrawIndexed(cmd, indexCount, instanceCount, 0, 0, 0);
```

---

## API Class Sketch

```cpp
class ShaderResourceBuilder {
public:
    ShaderResourceBuilder(const ShaderReflection& reflection, VkDevice device);
    void Set(const std::string& fieldPath, const ValueType& value); // For scalars, vectors, matrices, etc.
    void SetTexture(const std::string& arrayName, uint32_t index, VkImageView view, VkSampler sampler); // For bindless arrays
    void SetTexture(const std::string& name, VkImageView view, VkSampler sampler); // For single textures
    ArrayProxy Array(const std::string& arrayName); // Ergonomic array access
    StructProxy Struct(const std::string& structName); // Ergonomic struct access
    void Finalize(); // Allocates, uploads, creates all descriptor sets
    void BindAll(VkCommandBuffer cmd, VkPipelineLayout layout);
    // ...
};

class ArrayProxy {
public:
    void Resize(size_t count);
    StructProxy operator[](size_t idx);
    // ...
};

class StructProxy {
public:
    void Set(const std::string& field, const ValueType& value);
    void SetArrayElement(const std::string& arrayField, uint32_t idx, const ValueType& value);
    StructProxy Struct(const std::string& field);
    // ...
};
```

---

## Notes
- All resource and parameter setup is done in one place, using dot/bracket notation for nested/array fields.
- `Finalize()` allocates all GPU memory, uploads all data, and creates all descriptor sets in the correct order.
- `BindAll()` binds all sets for you—no manual descriptor set management.
- This API is ideal for simple/static pipelines or prototyping, and can be extended for more advanced use cases if needed.
