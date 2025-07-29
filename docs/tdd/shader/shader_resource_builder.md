# Shader Buffer & Resource Builder API Sketch

This document sketches a reflection-driven, builder-style C++ interface for allocating GPU buffers and texture arrays for Vulkan, matching a GLSL shader layout. It demonstrates how to:
- Use reflection to access and fill out buffer members (arrays/structs) defined in the shader
- Defer GPU allocation until all CPU-side data is ready
- Allocate and bind resources in a way that matches the shader's descriptor set layout

---

## Example GLSL Shader

```glsl
// Example: Bindless Material System
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

layout(buffer_reference, std430) buffer LightingParams {
    vec3 lightDirection;
    vec4 ambientColor;
};

layout(buffer_reference, std430) buffer MaterialParams {
    vec4 baseColor;
    float roughness;
    float metallic;
    LightingParams lighting; // Optional, may be null
    uint textureIndices[4];
};

struct InstanceData {
    mat4 model;
    MaterialParams material;
};

struct ConfigData {
    int maxInstances;
    float globalWeight;
};
```

---

## C++ API Sketch

bindlessTextures.BindToDescriptor(descriptorSet, set=1, binding=0);

### 1. BufferBuilder: CPU-Side Staging (with Nested Structs)

```cpp
// Reflection is loaded from SPIR-V
ShaderReflection reflection = LoadShaderReflection("shader.spv");

// Create a builder for the instance buffer defined in the shader
BufferBuilder instanceBuilder("InstanceBuffer", reflection);

// Get references to the array and struct in InstanceBuffer
auto& instances = instanceBuilder.GetArray("instances");
instances.Resize(instanceCount);
auto& config = instanceBuilder.GetStruct("config");

// Fill out array and struct
for (size_t i = 0; i < instanceCount; ++i) {
    instances[i].Set("model", glm::mat4(...));

    // Allocate MaterialParams for this instance
    BufferBuilder materialBuilder("MaterialParams", reflection);
    auto& matStruct = materialBuilder.GetStruct(""); // root struct
    matStruct.Set("baseColor", glm::vec4(...));
    matStruct.Set("roughness", 0.5f);
    matStruct.Set("metallic", 1.0f);

    // Allocate and fill nested LightingParams (buffer reference inside MaterialParams)
    BufferBuilder lightingBuilder("LightingParams", reflection);
    auto& lightStruct = lightingBuilder.GetStruct("");
    lightStruct.Set("lightDirection", glm::vec3(1,0,0));
    lightStruct.Set("ambientColor", glm::vec4(0.1f,0.1f,0.1f,1.0f));
    GpuBuffer lightingParams = lightingBuilder.Finalize(device);

    // Set buffer reference in MaterialParams
    matStruct.SetReference("lighting", lightingParams);

    // Set texture indices (from texture array builder, see below)
    matStruct.Set("textureIndices[0]", albedoIdx);
    matStruct.Set("textureIndices[1]", normalIdx);

    GpuBuffer materialParams = materialBuilder.Finalize(device);

    // Set buffer reference in InstanceData
    instances[i].SetReference("material", materialParams);
}

// Fill out config struct
config.Set("maxInstances", static_cast<int>(instanceCount));
config.Set("globalWeight", 1.0f);

// Once all data is filled out, allocate the GPU buffer:
GpuBuffer instanceBuffer = instanceBuilder.Finalize(device); // Allocates, uploads, returns handle
```

#### Example: Setting Nested Struct Fields (Embedded, not buffer reference)

If a struct contains an embedded struct (not a buffer reference), you can set nested fields using dot notation:

```cpp
auto& matStruct = materialBuilder.GetStruct("");
matStruct.Set("nestedStruct.nestedField", value);
```

---

#### More Example Use Cases

#### Multiple Arrays and Structs in a Buffer

```cpp
BufferBuilder builder("MyBuffer", reflection);
auto& arrA = builder.GetArray("arrayA");
arrA.Resize(N);
auto& arrB = builder.GetArray("arrayB");
arrB.Resize(M);
auto& config = builder.GetStruct("config");
config.Set("foo", 42);
```

#### Setting Deeply Nested Fields

```cpp
auto& root = builder.GetStruct("");
root.Set("outer.inner.value", 123.0f);
```

#### Handling Optional Buffer References

```cpp
// If a buffer reference is optional, you can set it to null or skip setting it
matStruct.SetReference("lighting", nullptr); // or omit if not used
```

---

## 2. TextureBuilder
```cpp
TextureBuilder albedoBuilder("albedoTex", reflection);
albedoBuilder.SetTexture(obj.albedoView, obj.albedoSampler);
GpuTexture albedoTex = albedoBuilder.Finalize(device);

TextureBuilder normalBuilder("normalTex", reflection);
normalBuilder.SetTexture(obj.normalView, obj.normalSampler);
GpuTexture normalTex = normalBuilder.Finalize(device);

TextureBuilder ormBuilder("ormTex", reflection);
ormBuilder.SetTexture(obj.ormView, obj.ormSampler);
GpuTexture ormTex = ormBuilder.Finalize(device);
```

## 3. TextureArrayBuilder

```cpp
TextureArrayBuilder texBuilder("bindlessTextures", reflection);

// Add textures, get their indices
uint32_t albedoIdx = texBuilder.AddTexture(albedoView, albedoSampler);
uint32_t normalIdx = texBuilder.AddTexture(normalView, normalSampler);

// Once all data is filled out, allocate the GPU buffer:
// Note: This might not actually do anything for textures, since they are already allocated
GpuTextureArray bindlessTextures = texBuilder.Finalize(device);

```

## 4. Descriptor Set Builder

The Descriptor Set Builder provides a reflection-driven way to assemble descriptor sets from resources (textures, buffers, etc.) and ensures all bindings belong to the same set index. It validates set indices using reflection and produces a finalized descriptor set ready for use or swapping in a group.

### Example: Building Descriptor Sets for the Example GLSL

```cpp
// Set = 0: CameraParams
DescriptorSetBuilder cameraSetBuilder(reflection);
cameraSetBuilder.WriteResource("camera", cameraBuffer); // cameraBuffer is a GpuBuffer
GpuDescriptorSet globalSet = cameraSetBuilder.Finalize(device); // set=0

// Set = 1: Bindless texture array
DescriptorSetBuilder bindlessSetBuilder(reflection);
bindlessSetBuilder.WriteResource("bindlessTextures", bindlessTextures); // GpuTextureArray
GpuDescriptorSet bindlessSet = bindlessSetBuilder.Finalize(device); // set=1

// Set = 2: Per-object textures (albedo, normal, orm)
DescriptorSetBuilder objectSetBuilder(reflection);
objectSetBuilder.WriteResource("albedoTex", albedoTex);   // GpuTexture
objectSetBuilder.WriteResource("normalTex", normalTex);   // GpuTexture
objectSetBuilder.WriteResource("ormTex", ormTex);         // GpuTexture
GpuDescriptorSet objectSet = objectSetBuilder.Finalize(device); // set=2

// Set = 3: InstanceBuffer
DescriptorSetBuilder instanceSetBuilder(reflection);
instanceSetBuilder.WriteResource("InstanceBuffer", instanceBuffer); // GpuBuffer
GpuDescriptorSet instanceSet = instanceSetBuilder.Finalize(device); // set=3
```

> **Note:**  
> A GPU descriptor set groups together resources (such as buffers and textures) for binding to shader stages. In this API, the set index is automatically inferred from the resources written to the descriptor set. The implementation validates—using shader reflection data—that all written resources belong to the same set index as defined in GLSL. This validated set index is then stored in the `GpuDescriptorSet` object.

## 5. Descriptor Set Group: Swapping and Binding Changed Sets

```cpp
// Build all persistent sets up front
GpuDescriptorSet globalSet = ...;    // set=0
GpuDescriptorSet bindlessSet = ...;  // set=1
GpuDescriptorSet instanceSet = ...;  // set=3

// Build all per-object sets (set=2)
std::vector<GpuDescriptorSet> objectSets = ...;

// Create the group and add persistent sets
DescriptorSetGroup setGroup;
auto globalKey   = setGroup.Add(globalSet);    // set=0
auto bindlessKey = setGroup.Add(bindlessSet);  // set=1
auto instanceKey = setGroup.Add(instanceSet);  // set=3

// Add the first object set (set=2) and keep its key
auto objectSetKey = setGroup.Add(objectSets[0]);

// In the draw loop, swap the per-object set as needed
for (size_t i = 0; i < objectSets.size(); ++i) {
    setGroup.Replace(objectSetKey, objectSets[i]); // Swap in the new set for set=2
    setGroup.BindChanged(cmd, pipeline, pipelineLayout); // Only binds changed sets in correct order
    vkCmdDrawIndexed(cmd, sceneObjects[i].indexCount, 1, 0, 0, 0);
}
```

> **Note:** 
> The DescriptorSetGroup internally tracks set indices via reflection, so the user never needs to manage set indices or ordering directly.

---

## 6. API Class Sketches

```cpp
class BufferBuilder {
public:
    BufferBuilder(const std::string& bufferName, const ShaderReflection& refl);
    ArrayProxy GetArray(const std::string& memberName);
    StructProxy GetStruct(const std::string& memberName);
    GpuBuffer Finalize(VkDevice device);
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
    void SetReference(const std::string& field, const BufferReference& ref);
    // ...
};

class TextureArrayBuilder {
public:
    TextureArrayBuilder(const std::string& name, const ShaderReflection& refl);
    uint32_t AddTexture(VkImageView view, VkSampler sampler);
    GpuTextureArray Finalize(VkDevice device);
    // ...
};

class GpuBuffer {
public:
    // Uses reflection info to write to the correct set/binding in the provided descriptor set
    void WriteDescriptor(VkDescriptorSet descriptorSet);
    // ...
};

class GpuTextureArray {
public:
    void WriteDescriptor(VkDescriptorSet descriptorSet);
    // ...
};
```

---

## Notes
- All member access is validated and offset-calculated via reflection.
- No C++ struct mirroring is required; all layout is shader-driven.
- Only members with `set = #, binding = #` can be bound directly; buffer references are handled via device addresses in buffer data.
- This pattern supports dynamic, flexible, and robust resource management for modern Vulkan rendering.
