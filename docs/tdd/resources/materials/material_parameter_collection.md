# MaterialParameterCollection Class

## Purpose
The `MaterialParameterCollection` class provides a mechanism for sharing parameter data (such as lighting, fog, or global values) across multiple materials and material instances. It enables efficient GPU access to shared data blocks using buffer device addresses.

## Responsibilities
- Stores a block of shared parameters accessible by multiple materials.
- Manages a GPU buffer for the parameter data and provides its device address for buffer_reference usage in shaders.
- Provides API for setting and updating parameters by name, with optional validation.
- Supports hot-reloading and dynamic updates of shared data.

## Key API
```cpp
class MaterialParameterCollection {
public:
    void SetParameter(const std::string& name, const void* data, size_t size);
    void UploadToGPU(); // Uploads packed parameter data to GPU buffer
    VkDeviceAddress GetBufferAddress() const;
    // ...other methods as needed
private:
    VkBuffer m_buffer;           // GPU buffer for shared parameters
    VkDeviceAddress m_address;   // Device address for buffer_reference
    // Internal parameter storage and layout info
};
```

## Integration
- Referenced by `MaterialInstance` to provide access to shared parameters.
- Managed and updated by higher-level systems (e.g., global lighting manager).
- Uses RHI `BufferAllocator` for GPU buffer management.

## Example Usage
```cpp
auto globalParams = std::make_shared<MaterialParameterCollection>();
globalParams->SetParameter("lightDirection", &dir, sizeof(dir));
globalParams->SetParameter("ambientColor", &color, sizeof(color));
VkDeviceAddress addr = globalParams->GetBufferAddress();
// Typical workflow for setting parameters and uploading to GPU:

globalParams->SetParameter("lightDirection", &dir, sizeof(dir));
globalParams->SetParameter("ambientColor", &color, sizeof(color));
globalParams->UploadToGPU();

// Get device address for shader access
VkDeviceAddress addr = globalParams->GetBufferAddress();
// Pass addr to material instances or directly to shaders
```

## Shader Access Example
```glsl
layout(buffer_reference, std430) buffer GlobalParamsBlock {
    vec4 ambientColor;
    vec3 lightDirection;
    // ...
};

// In instance data or push constant:
GlobalParamsBlock globalParams;

// Usage in shader:
vec3 dir = globalParams.lightDirection;
```

## Notes
- Parameter layout should match between CPU and shader for correct access.
- Reflection or metadata can be used to validate parameter names and sizes.
- Multiple collections can be used for different shared data domains (e.g., lighting, fog, environment).
