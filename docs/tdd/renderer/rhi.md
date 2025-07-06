# RHI (Render Hardware Interface) Technical Design Document

## 1. Purpose and Rationale

The Render Hardware Interface (RHI) provides a minimal, testable, and dependency-injectable abstraction over platform-specific graphics APIs (e.g., Vulkan, Metal, DirectX, or mock backends). The RHI enables the renderer and other systems to be written against a stable API, allowing for:
- **Testability:** Mock or fake RHIs can be injected for unit/integration testing.
- **Dependency Injection:** The RHI implementation is provided to consumers (e.g., Renderer) via constructor or setter, not hardcoded.
- **Compile-Time or Runtime Backend Selection:** The RHI can be switched at compile time for maximum performance (no virtual calls in release), or at runtime via a virtual interface for flexibility (e.g., editor, multi-backend, or testing).

---

## 2. API Abstraction Pattern

The RHI exposes a minimal set of API-agnostic methods (e.g., `cmd_bind_pipeline`, `create_buffer`, etc.) that are implemented by each backend (Vulkan, Metal, Mock, etc.).

### Example API (C++)
```cpp
// Virtual interface (enabled only if multiple RHIs are enabled or for testing)
class IRHI {
public:
    virtual ~IRHI() = default;
    virtual void cmd_bind_pipeline(void* cmd, int bind_point, void* pipeline) = 0;
    // ...other RHI methods...
};

// Vulkan implementation
class VulkanAPI RHI_BASE /* : public IRHI if virtual */ {
public:
    void cmd_bind_pipeline(void* cmd, int bind_point, void* pipeline) RHI_OVERRIDE; /* override if virtual */;
    // ...
};

// DX12 implementation
class DX12API RHI_BASE /* : public IRHI if virtual */ {
public:
    void cmd_bind_pipeline(void* cmd, int bind_point, void* pipeline) RHI_OVERRIDE; /* override if virtual */;
    // ...
};


// Metal implementation
class MetalAPI RHI_BASE /* : public IRHI if virtual */ {
public:
    void cmd_bind_pipeline(void* cmd, int bind_point, void* pipeline) RHI_OVERRIDE; /* override if virtual */;
    // ...
};

// Mock implementation for testing
class MockRHI RHI_BASE /* : public IRHI if virtual */ {
public:
    void cmd_bind_pipeline(void* cmd, int bind_point, void* pipeline) RHI_OVERRIDE; /* override if virtual */;
    // ...
};
```

---

## 3. Compile-Time vs. Runtime Selection

- **Compile-Time Selection (Release/Production):**
  - Only one RHI backend is enabled (e.g., `USE_VULKAN_RHI=1`).
  - No virtual base class or vtable overhead; all calls are statically resolved.
  - Maximum performance, minimal binary size.
- **Runtime Selection (Debug/Testing):**
  - Multiple RHIs enabled, or none specified (`USE_VIRTUAL_RHI=1`).
  - A virtual base class (`IRHI`) is used, and all backends inherit from it.
  - Allows switching RHI at runtime, or injecting mocks for tests.

### Macro Pattern (see `compile_time_RHI_switch.cpp`)
- Macros select the base class and override specifier depending on build configuration:
    ```cpp
    #if USE_VIRTUAL_RHI
    #   define RHI_BASE : public IRHI
    #   define RHI_OVERRIDE override
    #else
    #   define RHI_BASE
    #   define RHI_OVERRIDE
    #endif
    ```
- The renderer and other consumers use `RHIBase` as the type, which resolves to the correct implementation at compile time.

---

## 4. Dependency Injection and Testability

- The RHI implementation is always injected into consumers (e.g., Renderer) via constructor or setter.
- For tests, a `MockRHI` can be injected, allowing for verification of API calls and state.
- No system should create or own the RHI directly; it should always be provided externally.

### Example (C++)
```cpp
class Renderer {
public:
    Renderer(RHIBase& rhi) : rhi(rhi) {}
    void record_draw_commands(void* cmd, void* pipeline) {
        rhi.cmd_bind_pipeline(cmd, 0, pipeline);
    }
private:
    RHIBase& rhi;
};
```

---

## 5. Example Usage

See `tests/compile_time_RHI_switch.cpp` for a full example of compile-time and runtime RHI switching, including:
- Virtual interface for multi-backend/testing
- Macro-based selection for static or dynamic dispatch
- MockRHI for test verification

---

## 6. Benefits
- **Performance:** No virtual overhead in production builds.
- **Testability:** Mock/fake RHIs can be injected for tests.
- **Flexibility:** Runtime switching for editor or multi-backend scenarios.
- **Decoupling:** Renderer and systems are not tied to a specific graphics API.

---

## 7. References
- [compile_time_RHI_switch.cpp](../../tests/compile_time_RHI_switch.cpp)
- [Dependency Injection in C++](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Ri-inject)
- [Mock Objects for Testing](https://martinfowler.com/articles/mocksArentStubs.html)

---

## 8. Current Implementation Notes

- **Supported Backends:**
  - Only the Mock RHI and Vulkan RHI are currently implemented and supported.
- **Data Types and Handles:**
  - All RHI-related data types and handles currently use Vulkan types directly.
  - This is a pragmatic choice for now, as abstracting Vulkan's POD/simple handle types into a more generic form (for use in a generic RHI function) is non-trivial and would require additional indirection or complexity.
  - As a result, RHI methods and data structures are currently Vulkan-centric, and future work may revisit this if/when additional backends are implemented.

This document should be updated as the RHI abstraction evolves or as new requirements arise.
