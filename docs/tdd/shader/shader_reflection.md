# ShaderReflection Class

## Purpose

The `ShaderReflection` class encapsulates the logic for extracting and storing reflection data from a SPIR-V shader module. It provides a clean interface for querying descriptor sets, bindings, push constants, and struct layouts, enabling the engine to build pipeline layouts and material parameter layouts robustly and efficiently.

---

## Responsibilities

- Load and parse SPIR-V binary data using a reflection library (e.g., SPIRV-Reflect)
- Extract descriptor set and binding information (types, counts, names)
- Extract push constant ranges
- Extract struct and buffer layouts (including nested structs)
- Provide accessors for all reflection data needed by pipeline and material systems

---

## Example Interface

```cpp
class ShaderReflection {
public:
    ShaderReflection(const std::vector<uint32_t>& spirv);
    // Query descriptor sets and bindings
    struct DescriptorBindingInfo {
        uint32_t set;
        uint32_t binding;
        std::string name;
        VkDescriptorType type;
        uint32_t count;
    };
    const std::vector<DescriptorBindingInfo>& GetDescriptorBindings() const;

    // Query push constant ranges
    struct PushConstantInfo {
        std::string name;
        uint32_t offset;
        uint32_t size;
    };
    const std::vector<PushConstantInfo>& GetPushConstants() const;

    // Query struct layout (e.g., for material parameter blocks)
    struct StructMemberInfo {
        std::string name;
        size_t offset;
        size_t size;
        // Optionally, type info, array size, etc.
    };
    std::vector<StructMemberInfo> GetStructLayout(const std::string& structName) const;

    // ...other helpers as needed...
};
```

---

## Example Usage

```cpp
// Load SPIR-V binary
std::vector<uint32_t> spirv = LoadSpirvFromFile("shader.frag.spv");
ShaderReflection reflection(spirv);

// Get descriptor bindings
for (const auto& binding : reflection.GetDescriptorBindings()) {
    printf("Set %u, Binding %u, Name: %s, Type: %d\n", binding.set, binding.binding, binding.name.c_str(), binding.type);
}

// Get push constants
for (const auto& pc : reflection.GetPushConstants()) {
    printf("Push constant: %s, Offset: %u, Size: %u\n", pc.name.c_str(), pc.offset, pc.size);
}

// Get struct layout
auto members = reflection.GetStructLayout("MaterialParams");
for (const auto& m : members) {
    printf("Member: %s, Offset: %zu, Size: %zu\n", m.name.c_str(), m.offset, m.size);
}
```

---

## Notes

- `ShaderReflection` should not own or keep the Vulkan shader module alive; it only needs the SPIR-V binary for reflection.
- Reflection data can be cached and reused as needed by the engine (e.g., in a ShaderReflectionCache).
- Use [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) or a similar library to implement the internals.
- This class is engine-side only and does not interact with Vulkan objects after pipeline creation.
