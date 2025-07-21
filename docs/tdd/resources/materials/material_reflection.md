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

## Key Structures

```cpp
#include <variant>
#include <vector>
#include <string>
#include <unordered_map>

// Type aliases for clarity
using ScalarOrVectorFormat = VkFormat; // For scalar/vector types
struct StructParameterInfo;            // Forward declaration for struct members
using StructMembers = std::vector<StructParameterInfo>; // For struct parameters
using BufferReferenceTypeName = std::string;            // For buffer reference types

// The variant type for parameter kinds
using ParameterKind = std::variant<
    ScalarOrVectorFormat,   // Scalar or vector
    StructMembers,          // Struct
    BufferReferenceTypeName // Buffer reference
>;

struct MaterialParameterInfo {
    std::string name;
    size_t offset;
    size_t size;
    ParameterKind kind;
};

struct StructParameterInfo {
    std::string name;
    size_t offset;
    size_t size;
    std::vector<MaterialParameterInfo> members;
};

// Full reflection info for a material shader
struct MaterialReflection {
    std::vector<MaterialParameterInfo> parameters; // Flat list (including structs/collections)
    size_t structSize; // Total size in bytes of the packed material struct

    // Bindless texture support
    size_t textureIndexCount;    // Number of expected texture indices in the struct
    size_t textureIndicesOffset; // Offset in struct where texture indices array begins

    // Map from buffer reference type name to full parameter reflection info for that collection
    std::unordered_map<std::string, std::vector<MaterialParameterInfo>> parameterCollectionReflections;

    // --- Validation Methods ---

    // Validate a parameter by name and size
    bool ValidateParameter(const std::string& name, size_t size) const {
        auto it = std::find_if(parameters.begin(), parameters.end(),
            [&](const MaterialParameterInfo& p) { return p.name == name; });
        if (it == parameters.end() || it->size != size) return false;
        // Additional type checks can be added here
        return true;
    }

    // Retrieve parameter info by name
    const MaterialParameterInfo* GetParameterInfo(const std::string& name) const {
        auto it = std::find_if(parameters.begin(), parameters.end(),
            [&](const MaterialParameterInfo& p) { return p.name == name; });
        return (it != parameters.end()) ? &(*it) : nullptr;
    }

    // Validate a parameter collection (buffer reference)
    bool ValidateParameterCollection(const std::string& name, const MaterialParameterCollection& collection) const {
        auto it = std::find_if(parameters.begin(), parameters.end(),
            [&](const MaterialParameterInfo& p) { return p.name == name && std::holds_alternative<BufferReferenceTypeName>(p.kind); });
        if (it == parameters.end()) return false;

        const auto* typeName = std::get_if<BufferReferenceTypeName>(&it->kind);
        if (!typeName || *typeName != collection.GetStructTypeName()) return false;

        auto refIt = parameterCollectionReflections.find(*typeName);
        if (refIt == parameterCollectionReflections.end()) return false;
        const auto& expectedParams = refIt->second;
        return collection.ValidateAgainst(expectedParams);
    }

    // Validate a texture index
    bool ValidateTextureIndex(uint32_t index) const {
        // Ensure the index is within the expected range
        return index < textureIndexCount;
    }
};
```

---

## Reflection and Validation

- When setting a parameter (including struct or buffer reference), the system:
  - Looks up the parameter by name in `parameters`
  - Uses `std::holds_alternative` or `std::get_if` to determine the parameter type
  - Validates type, size, and (for structs/collections) struct type name
  - For buffer references (parameter collections), checks that the provided buffer's struct type matches the expected type
  - Packs the data or address at the correct offset in the material's CPU-side buffer

- When setting a parameter collection, the system:
  - Looks up the buffer reference parameter in `parameters` by name and checks that its type is `BufferReferenceTypeName`
  - Looks up the buffer reference type in `parameterCollectionReflections`
  - Validates the provided collection's internal data using the reflection info for that type

- When setting textures, the system:
  - Expects an array of bindless texture indices (uint32_t) to be set at the correct offset in the struct
  - Validates that the number of indices matches `textureIndexCount`
  - Packs the indices at `textureIndicesOffset` in the struct

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
    parameters = {
        { "baseColor", 0, 16, ScalarOrVectorFormat(VK_FORMAT_R32G32B32A32_SFLOAT) },
        { "roughness", 16, 4, ScalarOrVectorFormat(VK_FORMAT_R32_SFLOAT) },
        { "lighting", 20, 8, BufferReferenceTypeName("LightingParams") },
        { "fog", 28, 8, BufferReferenceTypeName("FogParams") },
        { "textureIndices", 36, 16, ScalarOrVectorFormat(VK_FORMAT_R32_UINT) }
    },
    structSize = 52, // Example value, must match std430 layout
    textureIndexCount = 4,
    textureIndicesOffset = 36,
    parameterCollectionReflections = {
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

- **All validation logic** for parameters and parameter collections is centralized in `MaterialReflection`.
- **MaterialInstance** is responsible only for calling validation and, if successful, copying data to the correct offset.
- **Parameter collections** are validated by type name and their internal layout, using a map for fast lookup.
- **Textures** are set by index, validated against the shader's expected count.
- **Struct and nested parameter layouts** are handled recursively.
- **CPU-side packing** must match the GPU-side layout exactly (std430, alignment, etc).

---

## Tools

- [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) or similar tools can extract struct layouts, buffer references, array sizes, and member info from SPIR-V.
- Optionally, generate JSON or metadata files for fast loading and validation.

---

## Summary

- The reflection system fully describes all material parameters, including scalars, vectors, structs, buffer references (parameter collections), and the expected number of texture indices.
- All validation is centralized in `MaterialReflection`, keeping `MaterialInstance` simple and robust.
- This enables robust, flexible, and safe material parameter management for both rasterization and compute/ray tracing shaders.

