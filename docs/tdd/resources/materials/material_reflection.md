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

## Typical Workflow

## Practical Usage: Validation, Packing, and GPU Upload

### Parameter Setting and Validation
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

### MaterialInstance GPU Upload
```cpp
// When parameters change, or before rendering:
void MaterialInstance::UploadToGPU(BufferAllocator* allocator) {
    if (!m_dirty) return;
    // Upload packed parameter data to GPU buffer
    allocator->UpdateBuffer(m_buffer, m_parameterData.data(), m_parameterData.size());
    m_dirty = false;
}

// To get device address for shader:
VkDeviceAddress MaterialInstance::GetParameterBufferAddress() const {
    return allocator->GetDeviceAddress(m_buffer);
}
```

### MaterialParameterCollection: Setting & Upload
```cpp
// Setting a shared parameter:
void MaterialParameterCollection::SetParameter(const std::string& name, const void* data, size_t size) {
    // Find offset from reflection/metadata
    auto it = paramLayout.find(name);
    if (it == paramLayout.end() || it->second.size != size) return;
    memcpy(m_data.data() + it->second.offset, data, size);
    m_dirty = true;
}

// Upload to GPU (called before frame or when dirty):
void MaterialParameterCollection::UploadToGPU(BufferAllocator* allocator) {
    if (!m_dirty) return;
    allocator->UpdateBuffer(m_buffer, m_data.data(), m_data.size());
    m_dirty = false;
}

VkDeviceAddress MaterialParameterCollection::GetBufferAddress() const {
    return allocator->GetDeviceAddress(m_buffer);
}
```

### Usage Example (Frame Setup)
```cpp
// Set parameters
matInstance->SetParameter("baseColor", &color, sizeof(color));
matInstance->SetTexture("albedoMap", texHandle);
matInstance->SetParameterCollection(globalParams);

// Upload to GPU before rendering
matInstance->UploadToGPU(bufferAllocator);
globalParams->UploadToGPU(bufferAllocator);

// Build instance data for GPU
InstanceData instance;
instance.model = ...;
instance.materialAddress = matInstance->GetParameterBufferAddress();
instance.globalParamsAddress = globalParams->GetBufferAddress();
```

// Reflection info is used to validate and pack parameters into a CPU-side buffer.
// On change or before rendering, packed data is uploaded to a GPU buffer.
// Device address of the buffer is provided for bindless access in the shader.
// MaterialParameterCollection works the same way, with its own buffer and address.

## Integration
- Used by MaterialRegistry and MaterialInstance for validation and packing.
- Enables strict CPU-side validation and debug checks.
- Supports hot-reloading and dynamic material layouts.

## Tools
- [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) recommended for extracting reflection info.

## Example Usage
```cpp
const MaterialReflection* refl = reflectionDB->Get("Shaders/PBR.frag.spv");
if (refl) {
    // Use refl->parameters and refl->textureNames for validation and packing
}
```
