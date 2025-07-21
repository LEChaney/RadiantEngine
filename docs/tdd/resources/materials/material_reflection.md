# Material Reflection System

## Purpose

The Material Reflection system parses shader metadata (typically SPIR-V) to extract the full layout of material parameters, including:
- Scalar and vector parameters
- Texture indices (for bindless arrays)
- Buffer reference types (parameter collections)
- Structs and nested parameter layouts

It enables strict validation and packing of parameter data on the CPU, ensuring all material and parameter collection data matches the expected GPU-side layout. This supports robust, flexible material systems with multiple parameter collections and struct-based parameters.

---

## Responsibilities

- Parse SPIR-V or metadata to extract:
  - Parameter names, types, offsets, and sizes (including struct/nested layouts)
  - Texture index array size and offset
  - Buffer reference types (parameter collections), including their expected struct type and layout
  - For each buffer reference type, extract and store full parameter reflection info for the referenced struct
- Provide per-material reflection info for validation and buffer packing
- Centralize all validation logic for parameters and parameter collections
- Support dynamic layouts for different material types and shaders

---

## Key Classes

```cpp
// Type aliases for clarity
using ScalarOrVectorFormat = VkFormat; // For scalar/vector types
struct StructParameterInfo;            // Forward declaration for struct members
using BufferReferenceTypeName = std::string; // For buffer reference types

// The variant type for parameter kinds
using ParameterTypeInfo = std::variant<
    ScalarOrVectorFormat,   // Scalar or vector
    StructParameterInfo,    // Struct
    BufferReferenceTypeName // Buffer reference
>;

struct MaterialParameterInfo {
    std::string name;
    size_t offset;
    size_t size;
    ParameterTypeInfo type_info;
};

struct StructParameterInfo {
    std::string name;
    std::vector<MaterialParameterInfo> members;
};

class MaterialReflection {
public:
    // Constructor: builds reflection info from a ShaderReflection object and struct name
    MaterialReflection(const ShaderReflection& shaderReflection, const std::string& structName);

    // Validation API
    bool ValidateParameter(const std::string& name, size_t size) const;
    const MaterialParameterInfo* GetParameterInfo(const std::string& name) const;
    bool ValidateParameterCollection(const std::string& name, const MaterialParameterCollection& collection) const;
    bool ValidateTextureIndex(uint32_t index) const;

    // Accessors
    size_t GetStructSize() const { return m_structSize; }
    size_t GetTextureIndexCount() const { return m_textureIndexCount; }
    size_t GetTextureIndicesOffset() const { return m_textureIndicesOffset; }
    const std::vector<MaterialParameterInfo>& GetParameters() const { return m_parameters; }
    const std::unordered_map<std::string, std::vector<MaterialParameterInfo>>& GetParameterCollectionReflections() const { return m_parameterCollectionReflections; }

private:
    std::vector<MaterialParameterInfo> m_parameters; // Flat list (including structs/collections)
    size_t m_structSize = 0; // Total size in bytes of the packed material struct

    // Bindless texture support
    size_t m_textureIndexCount = 0;    // Number of expected texture indices in the struct
    size_t m_textureIndicesOffset = 0; // Offset in struct where texture indices array begins

    // Map from buffer reference type name to full parameter reflection info for that collection
    std::unordered_map<std::string, std::vector<MaterialParameterInfo>> m_parameterCollectionReflections;

    // Internal helpers for reflection parsing and validation
    void ParseStruct(const ShaderReflection& shaderReflection, const std::string& structName);
    void ParseParameterCollection(const ShaderReflection& shaderReflection, const std::string& typeName);
};
```

---

## Reflection and Validation

- When setting a parameter (including struct or buffer reference), the system:
  - Looks up the parameter by name in `m_parameters`
  - Uses `std::holds_alternative` or `std::get_if` to determine the parameter type
  - Validates type, size, and (for structs/collections) struct type name
  - For buffer references (parameter collections), checks that the provided buffer's struct type matches the expected type
  - Packs the data or address at the correct offset in the material's CPU-side buffer

- When setting a parameter collection, the system:
  - Looks up the buffer reference parameter in `m_parameters` by name and checks that its type is `BufferReferenceTypeName`
  - Looks up the buffer reference type in `m_parameterCollectionReflections`
  - Validates the provided collection's internal data using the reflection info for that type

- When setting textures, the system:
  - Expects an array of bindless texture indices (uint32_t) to be set at the correct offset in the struct
  - Validates that the number of indices matches `m_textureIndexCount`
  - Packs the indices at `m_textureIndicesOffset` in the struct

---

## Example: Reflection for Structs, Parameter Collections, and Texture Indices

Suppose your shader has:

```glsl
layout(buffer_reference, std430) buffer LightingParams {
    vec3 lightDir;
    vec4 ambient;
};

layout(buffer_reference, std430) buffer FogParams {
    float fogDensity;
    vec3 fogColor;
};

layout(std430) buffer MaterialParams {
    vec4 baseColor;
    float roughness;
    LightingParams lighting; // buffer reference
    FogParams fog;           // buffer reference
    uint textureIndices[4];
};
```

The reflection system would produce:

```cpp
MaterialReflection {
    // m_parameters:
    {
        { "baseColor", 0, 16, ScalarOrVectorFormat(VK_FORMAT_R32G32B32A32_SFLOAT) },
        { "roughness", 16, 4, ScalarOrVectorFormat(VK_FORMAT_R32_SFLOAT) },
        { "lighting", 20, 8, BufferReferenceTypeName("LightingParams") },
        { "fog", 28, 8, BufferReferenceTypeName("FogParams") },
        { "textureIndices", 36, 16, ScalarOrVectorFormat(VK_FORMAT_R32_UINT) }
    },
    m_structSize = 52, // Example value, must match std430 layout
    m_textureIndexCount = 4,
    m_textureIndicesOffset = 36,
    m_parameterCollectionReflections = {
        { "LightingParams", {
            { "lightDir", 0, 12, ScalarOrVectorFormat(VK_FORMAT_R32G32B32_SFLOAT) },
            { "ambient", 16, 16, ScalarOrVectorFormat(VK_FORMAT_R32G32B32A32_SFLOAT) }
        }},
        { "FogParams", {
            { "fogDensity", 0, 4, ScalarOrVectorFormat(VK_FORMAT_R32_SFLOAT) },
            { "fogColor", 16, 12, ScalarOrVectorFormat(VK_FORMAT_R32G32B32_SFLOAT) }
        }}
    }
}
```

---

## Notes

- All validation logic for parameters and parameter collections is centralized in `MaterialReflection`.
- `MaterialInstance` is responsible only for calling validation and, if successful, copying data to the correct offset.
- Parameter collections are validated by type name and their internal layout, using a map for fast lookup.
- Textures are set by index, validated against the shader's expected count.
- Struct and nested parameter layouts are handled recursively.
- CPU-side packing must match the GPU-side layout exactly (std430, alignment, etc).

---

## Tools

- [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) or similar tools can extract struct layouts, buffer references, array sizes, and member info from SPIR-V.
- Optionally, generate JSON or metadata files for fast loading and validation.

---

## Summary

- The reflection system fully describes all material parameters, including scalars, vectors, structs, buffer references (parameter collections), and the expected number of texture indices.
- All validation is centralized in `MaterialReflection`, keeping `MaterialInstance` simple and robust.
- This enables robust, flexible, and safe material parameter management for both rasterization and compute/ray tracing shaders.

