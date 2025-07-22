
#include <gtest/gtest.h>
#include "rhi/rhi_context.h"
#include "rhi/command_buffer.h"
#include "rhi/queue.h"
#include "rhi/swapchain.h"
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_swapchain.h"
#include "fmt/format.h"
#include <vector>

#include <SDL.h>

using rhi::RHIContext;
using rhi::vulkan::RHIVKContext;

// Helper to capture validation messages
static std::vector<std::string> g_capturedValidationMsgs;

void ValidationMsgCollector(const char* msg, RHIVKContext::ValidationLevel level) {
    const char* levelStr = "INFO";
    if (level == RHIVKContext::ValidationLevel::Error) {
        levelStr = "ERROR";
    } else if (level == RHIVKContext::ValidationLevel::Warning) {
        levelStr = "WARNING";
    } else {
        levelStr = "INFO";
    }
    std::string formatted_msg = fmt::format("[Validation {}] {}", levelStr, msg);
    //std::cerr << formatted_msg << std::endl;
    g_capturedValidationMsgs.emplace_back(formatted_msg);
}

class RHIVulkanTest : public ::testing::Test {
protected:
    RHIContext* context = nullptr;

    void SetUp() override {
        g_capturedValidationMsgs.clear();
        RHIVKContext::set_validation_callback(ValidationMsgCollector);
        context = new RHIVKContext(true); // enable validation for test
        ASSERT_NE(context, nullptr);
    }

    void TearDown() override {
        delete context;
        context = nullptr;
        RHIVKContext::set_validation_callback(nullptr);
        g_capturedValidationMsgs.clear();
    }
};

class RHIVulkanTestWithSDLAndSwap : public RHIVulkanTest {
protected:
    SDL_Window* window = nullptr;
    rhi::RHISwapchain* swapchain = nullptr;
    const uint32_t buffer_count = 2; // double buffering

    void SetUp() override {
        RHIVulkanTest::SetUp();

        // Initialize SDL
        ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0);

        const int width = 640, height = 480;
        window = SDL_CreateWindow(
            "RHI Vulkan Swapchain Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
        ASSERT_NE(window, nullptr);

        // Create swapchain (API assumed: create_swapchain(SDL_Window*, width, height, buffer_count))
        swapchain = context->create_swapchain(window, width, height, buffer_count);
        ASSERT_NE(swapchain, nullptr);
        EXPECT_EQ(swapchain->image_count(), buffer_count);
    }

    void TearDown() override {
        if (swapchain) {
            delete swapchain;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }

        SDL_Quit();
        RHIVulkanTest::TearDown();
    }
};

TEST_F(RHIVulkanTest, BasicInitTeardown) {
    for (const auto& msg : g_capturedValidationMsgs) {
        EXPECT_EQ(msg, "");
    }
    EXPECT_TRUE(g_capturedValidationMsgs.empty());
}

TEST_F(RHIVulkanTest, CreateContextAndGraphicsQueue) {
    auto* queue = context->get_graphics_queue();
    ASSERT_NE(queue, nullptr);
    for (const auto& msg : g_capturedValidationMsgs) {
        EXPECT_EQ(msg, "");
    }
    EXPECT_TRUE(g_capturedValidationMsgs.empty());
}

TEST_F(RHIVulkanTest, CreateAndSubmitEmptyCommandBuffer) {
    auto* queue = context->get_graphics_queue();
    ASSERT_NE(queue, nullptr);
    auto* cmd = context->create_command_buffer();
    ASSERT_NE(cmd, nullptr);
    cmd->begin();
    cmd->end();
    queue->submit({cmd}, nullptr, nullptr);
    queue->wait_idle();
    for (const auto& msg : g_capturedValidationMsgs) {
        EXPECT_EQ(msg, "");
    }
    EXPECT_TRUE(g_capturedValidationMsgs.empty());
}

TEST_F(RHIVulkanTestWithSDLAndSwap, SwapchainDoubleBufferingDrawWithSDL) {
    // Draw two frames (double buffering)
    for (uint32_t frame = 0; frame < buffer_count; ++frame) {
        auto frame_data = swapchain->acquire_next_frame();
        ASSERT_NE(frame_data.command_buffer, nullptr);
        ASSERT_NE(frame_data.image_view, nullptr);

        frame_data.command_buffer->begin();
        // Pseudo-code: clear the image to a color (adapt to your API)
        // frame_data.command_buffer->clear_color(frame_data.image_view, {0.1f * frame, 0.2f, 0.3f, 1.0f});
        frame_data.command_buffer->end();

        swapchain->present(frame_data);
    }
}
