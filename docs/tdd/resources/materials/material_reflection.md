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
- Enable validation when setting any parameter, texture index array, or parameter collection (including type and struct matching)
- Support dynamic layouts for different material types and shaders

---

## Key Structures

```cpp
// Describes a single parameter (scalar, vector, struct, or buffer reference)
struct MaterialParameterInfo {
    std::string name;
    size_t offset;
    size_t size;
    VkFormat format; // For scalars/vectors
    bool isStruct = false;
    bool isBufferReference = false;
    std::string structTypeName; // For structs or buffer references
    std::vector<MaterialParameterInfo> members; // For struct members
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
};
```

---

## Reflection and Validation

- When setting a parameter (including struct or buffer reference), the system:
  - Looks up the parameter by name in `parameters`
  - Validates type, size, and (for structs/collections) struct type name
  - For buffer references (parameter collections), checks that the provided buffer's struct type matches the expected type
  - Packs the data or address at the correct offset in the material's CPU-side buffer

- When setting a parameter collection, the system:
  - Looks up the buffer reference parameter in `parameters` by name and checks `isBufferReference`
  - Looks up the buffer reference type in `parameterCollectionReflections`
  - Validates the provided collection's internal data using the reflection info for that type

- When setting textures, the system:
  - Expects an array of bindless texture indices (uint32_t) to be set at the correct offset in the struct
  - Validates that the number of indices matches `textureIndexCount`
  - Packs the indices at `textureIndicesOffset` in the struct

---

## Example: Setting Parameters, Parameter Collections, and Texture Indices

```cpp
// Setting a scalar/vector/struct parameter
bool MaterialInstance::SetParameter(const std::string& name, const void* data, size_t size) {
    const MaterialReflection* refl = material->GetReflection();
    auto it = std::find_if(refl->parameters.begin(), refl->parameters.end(),
        [&](const MaterialParameterInfo& p) { return p.name == name; });
    if (it == refl->parameters.end() || it->size != size) return false; // Validation

    memcpy(m_parameterData.data() + it->offset, data, size);
    m_dirty = true;
    return true;
}

// Setting a parameter collection (buffer reference)
bool MaterialInstance::SetParameterCollection(const std::string& name, const MaterialParameterCollection& collection) {
    const MaterialReflection* refl = material->GetReflection();
    auto it = std::find_if(refl->parameters.begin(), refl->parameters.end(),
        [&](const MaterialParameterInfo& p) { return p.name == name && p.isBufferReference; });
    if (it == refl->parameters.end()) return false;

    // Validate struct type matches
    if (it->structTypeName != collection.GetStructTypeName()) return false;

    // Validate the collection's internal data layout using parameterCollectionReflections
    auto refIt = refl->parameterCollectionReflections.find(it->structTypeName);
    if (refIt == refl->parameterCollectionReflections.end()) return false;
    const auto& expectedParams = refIt->second;
    if (!collection.ValidateAgainst(expectedParams)) return false;

    // Write the buffer device address at the correct offset
    VkDeviceAddress addr = collection.GetBufferAddress();
    memcpy(m_parameterData.data() + it->offset, &addr, sizeof(addr));
    m_dirty = true;
    return true;
}

// Setting a single texture index (bindless)
bool MaterialInstance::SetTexture(TextureHandle handle, uint32_t index) {
    const MaterialReflection* refl = material->GetReflection();
    if (index >= refl->textureIndexCount) return false; // Validation

    uint32_t bindlessIndex = textureRegistry->GetBindlessIndex(handle);
    memcpy(m_parameterData.data() + refl->textureIndicesOffset + index * sizeof(uint32_t), &bindlessIndex, sizeof(uint32_t));
    m_dirty = true;
    return true;
}
```

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
        { "baseColor", 0, 16, VK_FORMAT_R32G32B32A32_SFLOAT },
        { "roughness", 16, 4, VK_FORMAT_R32_SFLOAT },
        { "lighting", 20, 8, VK_FORMAT_UNDEFINED, false, true, "LightingParams" },
        { "fog", 28, 8, VK_FORMAT_UNDEFINED, false, true, "FogParams" },
        { "textureIndices", 36, 16, VK_FORMAT_R32_UINT }
    },
    structSize = 52, // Example value, must match std430 layout
    textureIndexCount = 4,
    textureIndicesOffset = 36,
    parameterCollectionReflections = {
        { "LightingParams", {
            { "lightDir", 0, 12, VK_FORMAT_R32G32B32_SFLOAT },
            { "ambient", 16, 16, VK_FORMAT_R32G32B32A32_SFLOAT }
        }},
        { "FogParams", {
            { "fogDensity", 0, 4, VK_FORMAT_R32_SFLOAT },
            { "fogColor", 16, 12, VK_FORMAT_R32G32B32_SFLOAT }
        }}
    }
}
```

---

## Notes

- **Parameter collections** are validated by checking for a buffer reference parameter in the parameter list and then using the type name to look up the expected layout in `parameterCollectionReflections`.
- **Multiple parameter collections** are supported; each is validated by name and struct type.
- **Struct parameters** (non-buffer reference) are recursively validated and packed.
- **Reflection system** must parse nested structs, buffer reference types, and array sizes from SPIR-V or metadata.
- **CPU-side packing** must match the GPU-side layout exactly (std430, alignment, etc).
- **Textures** are set by index array, validated against the shader's expected count.

---

## Tools

- [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) or similar tools can extract struct layouts, buffer references, array sizes, and member info from SPIR-V.
- Optionally, generate JSON or metadata files for fast loading and validation.

---

## Summary

- The reflection system fully describes all material parameters, including scalars, vectors, structs, buffer references (parameter collections), and the expected number of texture indices.
- Parameter collections are validated by type name and their internal layout, using a map for fast lookup.
- Textures are set by index array, validated against the shader's expected count.
- Struct and nested parameter layouts are handled recursively.
- This enables robust, flexible, and safe material parameter management for both rasterization and compute/ray tracing shaders.

