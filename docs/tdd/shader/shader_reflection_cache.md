# ShaderReflectionCache

## Purpose

The `ShaderReflectionCache` class manages and caches reflection data for SPIR-V shader modules. It ensures that reflection is only performed once per unique shader, avoids redundant work, and provides fast access to reflection data for pipelines, materials, and tools.

---

## Responsibilities

- Load and parse SPIR-V binaries as needed
- Store and cache `ShaderReflection` objects, indexed by file path, hash, or unique identifier
- Provide access to reflection data for any shader used in the engine
- Avoid redundant reflection work for shaders used in multiple pipelines/materials
- Optionally support hot-reloading or cache invalidation

---

## Example Interface

```cpp
class ShaderReflectionCache {
public:
    // Retrieve or load reflection data for a given SPIR-V path
    std::shared_ptr<ShaderReflection> GetOrLoad(const std::string& spirvPath);

    // Optionally, clear or reload cache entries
    void Clear();
    void Reload(const std::string& spirvPath);

private:
    std::unordered_map<std::string, std::shared_ptr<ShaderReflection>> m_reflections;
};
```

---

## Example Usage

```cpp
ShaderReflectionCache cache;
auto reflection = cache.GetOrLoad("shaders/pbr.frag.spv");
if (reflection) {
    // Use reflection data for pipeline/material setup
}
```

---

## Notes

- The cache should only store lightweight reflection data, not Vulkan shader modules or full SPIR-V binaries.
- Use file paths, hashes, or other unique identifiers as cache keys.
- This class is engine-side only and can be a singleton or a member of your resource/shader system.
- For hot-reloading, implement cache invalidation and reloading logic as needed.

---

## Summary

- `ShaderReflectionCache` centralizes and optimizes access to SPIR-V reflection data.
- It avoids redundant reflection work and supports efficient pipeline and material creation.
- Keeps your engine's reflection system robust, fast, and easy to maintain.
