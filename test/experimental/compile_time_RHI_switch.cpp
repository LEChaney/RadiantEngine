

#include <iostream>
#include <vector>
#include <tuple>
#include <cassert>

#ifndef USE_VULKAN_RHI
#   define USE_VULKAN_RHI 0
#endif
#ifndef USE_METAL_RHI
#   define USE_METAL_RHI 0
#endif
#ifndef USE_MOCK_RHI
#   define USE_MOCK_RHI 0
#endif

// For demonstration, you can comment/uncomment the desired mode:
// Enable USE_VIRTUAL_RHI only if more than one RHI is enabled or none is explicitly enabled
#define RHI_COUNT ( USE_VULKAN_RHI + \
                    USE_METAL_RHI + \
                    USE_MOCK_RHI )
#if RHI_COUNT != 1
#   define USE_VIRTUAL_RHI 1
#else
#   define USE_VIRTUAL_RHI 0
#endif

// Macro helpers for inheritance and virtual
#if USE_VIRTUAL_RHI
#   define RHI_BASE : public IRHI
#   define RHI_OVERRIDE override
#else
#   define RHI_BASE
#   define RHI_OVERRIDE
#endif

#if USE_VIRTUAL_RHI
// Interface for RHI
class IRHI {
public:
    virtual ~IRHI() = default;
    /**
     * @brief Binds a pipeline object to a command buffer at the specified bind point.
     *
     * This pure virtual function must be implemented by derived classes to bind a pipeline
     * (such as a graphics or compute pipeline) to the provided command buffer.
     *
     * @param cmd Pointer to the command buffer where the pipeline will be bound.
     * @param bindPoint Integer specifying the bind point (e.g., graphics or compute).
     * @param pipeline Pointer to the pipeline object to be bound.
     */
    virtual void CmdBindPipeline(void* cmd, int bindPoint, void* pipeline) = 0;
};
#endif

// VulkanAPI definition
class VulkanAPI RHI_BASE {
public:
    ~VulkanAPI() RHI_OVERRIDE = default;
    void CmdBindPipeline(void* cmd, int bindPoint, void* pipeline) RHI_OVERRIDE {
        std::cout << "VulkanAPI::CmdBindPipeline called\n";
    }
};

// MetalAPI definition
class MetalAPI RHI_BASE {
public:
    ~MetalAPI() RHI_OVERRIDE = default;
    void CmdBindPipeline(void* cmd, int bindPoint, void* pipeline) RHI_OVERRIDE {
        std::cout << "MetalAPI::CmdBindPipeline called\n";
    }
};

// Mock RHI for testing
class MockRHI RHI_BASE {
public:
    ~MockRHI() RHI_OVERRIDE = default;
    std::vector<std::tuple<void*, int, void*>> bindCalls;
    void CmdBindPipeline(void* cmd, int bindPoint, void* pipeline) RHI_OVERRIDE {
        bindCalls.emplace_back(cmd, bindPoint, pipeline);
        std::cout << "MockRHI::CmdBindPipeline called\n";
    }
};

#if USE_VIRTUAL_RHI
using RHIBase = IRHI;
#elif USE_VULKAN_RHI
using RHIBase = VulkanAPI;
#elif USE_METAL_RHI
using RHIBase = MetalAPI;
#elif USE_MOCK_RHI
using RHIBase = MockRHI;
#else
// Fallback if no RHI is defined
using RHIBase = VulkanAPI; // Default to VulkanAPI if no specific RHI is defined
#endif

// Renderer using the Vulkan API abstraction
class Renderer {
public:
    // Always use RHIBase for the member type
    Renderer(RHIBase& api) : rhi(api) {}

    void recordDrawCommands(void* cmd, void* pipeline) {
        rhi.CmdBindPipeline(cmd, /*bindPoint=*/0, pipeline);
    }

private:
    RHIBase& rhi;
};

int main() {
#if USE_VIRTUAL_RHI
    // Use production VulkanAPI via interface
    VulkanAPI prodApi;
    Renderer prodRenderer(prodApi);
    prodRenderer.recordDrawCommands((void*)0x1, (void*)0x2);

    // Use MetalAPI via interface
    MetalAPI metalApi;
    Renderer metalRenderer(metalApi);
    metalRenderer.recordDrawCommands((void*)0x5, (void*)0x6);

    // Use mock for testing
    MockRHI mockApi;
    Renderer mockRenderer(mockApi);
    mockRenderer.recordDrawCommands((void*)0x3, (void*)0x4);
    assert(mockApi.bindCalls.size() == 1);
    auto [cmd, bindPoint, pipeline] = mockApi.bindCalls[0];
    assert(cmd == (void*)0x3);
    assert(pipeline == (void*)0x4);
    std::cout << "Test passed!\n";
#elif USE_VULKAN_RHI
    VulkanAPI prodApi;
    Renderer prodRenderer(prodApi);
    prodRenderer.recordDrawCommands((void*)0x1, (void*)0x2);
#elif USE_METAL_RHI
    MetalAPI metalApi;
    Renderer metalRenderer(metalApi);
    metalRenderer.recordDrawCommands((void*)0x5, (void*)0x6);
#elif USE_MOCK_RHI
    MockRHI mockApi;
    Renderer mockRenderer(mockApi);
    mockRenderer.recordDrawCommands((void*)0x3, (void*)0x4);
    assert(mockApi.bindCalls.size() == 1);
    auto [cmd, bindPoint, pipeline] = mockApi.bindCalls[0];
    assert(cmd == (void*)0x3);
    assert(pipeline == (void*)0x4);
    std::cout << "Test passed!\n";
#endif
    return 0;
}
