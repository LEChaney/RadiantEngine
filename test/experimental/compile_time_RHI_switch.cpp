#include <iostream>
#include <gtest/gtest.h>
#include <vector>
#include <tuple>

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
     * @param bind_point Integer specifying the bind point (e.g., graphics or compute).
     * @param pipeline Pointer to the pipeline object to be bound.
     */
    virtual void cmd_bind_pipeline(void* cmd, int bind_point, void* pipeline) = 0;
};
#endif

// VulkanAPI definition

class VulkanAPI RHI_BASE {
public:
    ~VulkanAPI() RHI_OVERRIDE = default;
    void cmd_bind_pipeline(void* cmd, int bind_point, void* pipeline) RHI_OVERRIDE {
        std::cout << "VulkanAPI::cmd_bind_pipeline called\n";
    }
};

// MetalAPI definition

class MetalAPI RHI_BASE {
public:
    ~MetalAPI() RHI_OVERRIDE = default;
    void cmd_bind_pipeline(void* cmd, int bind_point, void* pipeline) RHI_OVERRIDE {
        std::cout << "MetalAPI::cmd_bind_pipeline called\n";
    }
};

// Mock RHI for testing

class MockRHI RHI_BASE {
public:
    ~MockRHI() RHI_OVERRIDE = default;
    std::vector<std::tuple<void*, int, void*>> bind_calls;
    void cmd_bind_pipeline(void* cmd, int bind_point, void* pipeline) RHI_OVERRIDE {
        bind_calls.emplace_back(cmd, bind_point, pipeline);
        std::cout << "MockRHI::cmd_bind_pipeline called\n";
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

    void record_draw_commands(void* cmd, void* pipeline) {
        rhi.cmd_bind_pipeline(cmd, /*bind_point=*/0, pipeline);
    }

private:
    RHIBase& rhi;
};


#if USE_VIRTUAL_RHI
TEST(rhi_virtual, vulkanapi_bind_pipeline) {
    VulkanAPI prod_api;
    Renderer prod_renderer(prod_api);
    // Should not throw or crash
    EXPECT_NO_THROW(prod_renderer.record_draw_commands((void*)0x1, (void*)0x2));
}

TEST(rhi_virtual, metalapi_bind_pipeline) {
    MetalAPI metal_api;
    Renderer metal_renderer(metal_api);
    EXPECT_NO_THROW(metal_renderer.record_draw_commands((void*)0x5, (void*)0x6));
}

TEST(rhi_virtual, mockrhi_bind_pipeline_and_record) {
    MockRHI mock_api;
    Renderer mock_renderer(mock_api);
    mock_renderer.record_draw_commands((void*)0x3, (void*)0x4);
    ASSERT_EQ(mock_api.bind_calls.size(), 1u);
    auto [cmd, bind_point, pipeline] = mock_api.bind_calls[0];
    EXPECT_EQ(cmd, (void*)0x3);
    EXPECT_EQ(pipeline, (void*)0x4);
}
#elif USE_VULKAN_RHI
TEST(rhi_vulkan, vulkanapi_bind_pipeline) {
    VulkanAPI prod_api;
    Renderer prod_renderer(prod_api);
    EXPECT_NO_THROW(prod_renderer.record_draw_commands((void*)0x1, (void*)0x2));
}
#elif USE_METAL_RHI
TEST(rhi_metal, metalapi_bind_pipeline) {
    MetalAPI metal_api;
    Renderer metal_renderer(metal_api);
    EXPECT_NO_THROW(metal_renderer.record_draw_commands((void*)0x5, (void*)0x6));
}
#elif USE_MOCK_RHI
TEST(rhi_mock, mockrhi_bind_pipeline_and_record) {
    MockRHI mock_api;
    Renderer mock_renderer(mock_api);
    mock_renderer.record_draw_commands((void*)0x3, (void*)0x4);
    ASSERT_EQ(mock_api.bind_calls.size(), 1u);
    auto [cmd, bind_point, pipeline] = mock_api.bind_calls[0];
    EXPECT_EQ(cmd, (void*)0x3);
    EXPECT_EQ(pipeline, (void*)0x4);
}
#endif
