# Material Reflection System

## Purpose

The Material Reflection system is responsible for parsing shader metadata (typically SPIR-V) to extract material parameter layouts, texture slots, and any shared parameter collections. It enables strict validation of parameter setting on the CPU side and ensures that material data is packed correctly for GPU access.

## Responsibilities
- Parse SPIR-V or metadata files to extract parameter names, types, offsets, and sizes.
- Provide per-shader reflection info for validation and buffer packing.
- Enable validation when setting material parameters and textures.
- Support dynamic layouts for different material types and shaders.

## Key Classes & Structures

### MaterialParameterInfo
```cpp
struct MaterialParameterInfo {
    std::string name;
    size_t offset;
    size_t size;
    VkFormat format;
};
```

### MaterialReflection
```cpp
struct MaterialReflection {
    std::vector<MaterialParameterInfo> parameters;
    std::vector<std::string> textureNames;
    size_t structSize; // total size in bytes
    // Optionally, info for parameter collections
};
```

### MaterialReflectionDB
```cpp
class MaterialReflectionDB {
public:
    // Returns reflection info for a given shader
    const MaterialReflection* Get(const std::string& shaderPath) const;
    // Loads/parses reflection info from SPIR-V or metadata
    // ...
};
```

## Example Usage
```cpp
// At material creation:
const MaterialReflection* refl = reflectionDB->Get("Shaders/PBR.frag.spv");
MaterialInstance instance(material);

// Setting a parameter:
bool MaterialInstance::SetParameter(const std::string& name, const void* data, size_t size) {
    // Find parameter info from reflection
    auto it = std::find_if(refl->parameters.begin(), refl->parameters.end(),
        [&](const MaterialParameterInfo& p) { return p.name == name; });
    if (it == refl->parameters.end() || it->size != size) return false; // Validation

    // Pack data into CPU-side buffer at correct offset
    memcpy(m_parameterData.data() + it->offset, data, size);
    m_dirty = true;
    return true;
}
```

## Tools
- [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) recommended for extracting reflection info.

