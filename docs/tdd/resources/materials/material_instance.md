## MaterialInstance Class

### Purpose
The `MaterialInstance` class represents a unique set of material parameters and texture bindings for a specific object or instance. It manages the actual parameter data, texture indices, and GPU buffer address for bindless access.

### Responsibilities
- Holds packed parameter data for a material instance, validated against reflection info.
- Manages texture bindings (indices into the bindless texture array).
- References shared parameter collections if needed.
- Allocates and manages a GPU buffer for parameter data, providing its device address for buffer_reference usage in shaders.
- Provides API for setting parameters and textures, with strict validation.

### Key API
```cpp
class MaterialInstance {
public:
    MaterialInstance(Material* material);

    bool SetParameter(const std::string& name, const void* data, size_t size);
    bool SetTexture(const std::string& name, TextureHandle handle);
    void SetParameterCollection(std::shared_ptr<MaterialParameterCollection> collection);

    void UploadToGPU(); // Uploads packed parameter data to GPU buffer
    VkDeviceAddress GetParameterBufferAddress() const; // buffer_reference for GPU
    const std::vector<uint32_t>& GetTextureIndices() const;
    // ...other per-instance info
    
};
```

### Integration
- Created per unique set of parameters/textures for an object.
- Uses `Material` for reflection info and pipeline reference.
- Uses `TextureRegistry` for texture indices.
- Uses RHI `BufferAllocator` for GPU buffer management.

### Example Usage
```cpp
auto matInstance = std::make_shared<MaterialInstance>(material);
matInstance->SetParameter("albedo", &albedo, sizeof(albedo));
matInstance->SetTexture("albedoMap", texHandle);
matInstance->SetParameterCollection(globalLightingParams);
VkDeviceAddress addr = matInstance->GetParameterBufferAddress();
// Typical workflow for parameter setting and GPU upload:

// Set parameters and textures
matInstance->SetParameter("baseColor", &color, sizeof(color));
matInstance->SetTexture("albedoMap", texHandle);
matInstance->SetParameterCollection(globalParams);

// Upload to GPU before rendering
matInstance->UploadToGPU();
globalParams->UploadToGPU();

// Build instance data for GPU
InstanceData instance;
instance.model = ...;
instance.materialAddress = matInstance->GetParameterBufferAddress();
instance.globalParamsAddress = globalParams->GetBufferAddress();
```

---

## Example: Setting Parameters, Parameter Collections, and Texture Indices

```cpp
bool MaterialInstance::SetParameter(const std::string& name, const void* data, size_t size) {
    const MaterialReflection* refl = material->GetReflection();
    if (!refl->ValidateParameter(name, size)) return false;
    const MaterialParameterInfo* paramInfo = refl->GetParameterInfo(name);
    // Optional: Check if parameter data is changing
    memcpy(m_parameterData.data() + paramInfo->offset, data, size);
    m_dirty = true;
    return true;
}

bool MaterialInstance::SetParameterCollection(const std::string& name, const MaterialParameterCollection& collection) {
    const MaterialReflection* refl = material->GetReflection();
    if (!refl->ValidateParameterCollection(name, collection)) return false;
    const MaterialParameterInfo* paramInfo = refl->GetParameterInfo(name);
    VkDeviceAddress addr = collection.GetBufferAddress();
    // Optional: Check if parameter data is changing
    memcpy(m_parameterData.data() + paramInfo->offset, &addr, sizeof(addr));
    m_dirty = true;
    return true;
}

bool MaterialInstance::SetTexture(TextureHandle handle, uint32_t index) {
    const MaterialReflection* refl = material->GetReflection();
    if (!refl->ValidateTextureIndex(index)) return false;

    uint32_t bindlessIndex = textureRegistry->GetBindlessIndex(handle);
    // Optional: Check if parameter data is changing
    memcpy(m_parameterData.data() + refl->textureIndicesOffset + index * sizeof(uint32_t), &bindlessIndex, sizeof(uint32_t));
    m_dirty = true;
    return true;
}
```
