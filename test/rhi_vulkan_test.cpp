
#include <gtest/gtest.h>
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/rhi_command_buffer.h"
#include "rhi/rhi_queue.h"
#include "rhi/rhi_swapchain.h"
#include "rhi/rhi_image_layout.h"
#include "rhi/rhi_fence.h"
#include "fmt/format.h"
#include "core/core_defs.h"

#include <SDL.h>

#include <algorithm>

#include "rhi/rhi_image_utils.h"

using rhi::RHIContext;
using rhi::vulkan::RHIVKContext;
using rhi::RHIImageView;
using rhi::ImageLayout;

// Helper to capture validation messages
void ValidationMsgCollector(const char* msg, RHIVKContext::ValidationLevel level) {
    const char* levelStr = "INFO";
    if (level == RHIVKContext::ValidationLevel::Error) {
        levelStr = "ERROR";
    } else if (level == RHIVKContext::ValidationLevel::Warning) {
        levelStr = "WARNING";
    } else {
        levelStr = "INFO";
    }
    std::string formatted_msg = fmt::format("[Vk Validation][{}] {}", levelStr, msg);
    GTEST_NONFATAL_FAILURE_(formatted_msg.c_str()); // Fail the test on validation errors
}

class RHIVulkanTest : public ::testing::Test {
protected:
    UniquePtr<RHIContext> context;

    void SetUp() override {
        RHIVKContext::set_validation_callback(ValidationMsgCollector);
        context = make_unique<RHIVKContext>(true); // enable validation for test
        ASSERT_NE(context, nullptr);
    }

    void TearDown() override {
        RHIVKContext::set_validation_callback(nullptr);
    }
};

class RHIVulkanTestWithSDLAndSwap : public RHIVulkanTest {
protected:
    SDL_Window* window = nullptr;
    UniquePtr<rhi::RHISwapchain> swapchain = nullptr;
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
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }

        SDL_Quit();
        RHIVulkanTest::TearDown();
    }
};

TEST_F(RHIVulkanTest, BasicInitTeardown) {
}

TEST_F(RHIVulkanTest, CreateContextAndGraphicsQueue) {
    auto* queue = context->get_graphics_queue();
    ASSERT_NE(queue, nullptr);
}

TEST_F(RHIVulkanTest, CreateAndSubmitEmptyCommandBuffer) {
    auto* queue = context->get_graphics_queue();
    ASSERT_NE(queue, nullptr);
    auto cmd = context->create_command_buffer();
    ASSERT_NE(cmd, nullptr);
    cmd->begin();
    cmd->end();
    queue->submit({cmd.get()}, nullptr, nullptr);
    queue->wait_idle();
}

TEST_F(RHIVulkanTestWithSDLAndSwap, RenderSwapchainFrames) {
    // Draw several frames (double buffered)
    constexpr uint32_t frame_count = 4;
    for (uint32_t frame_idx = 0; frame_idx < frame_count; ++frame_idx) {
        auto frame_data = swapchain->acquire_next_frame();
        ASSERT_NE(frame_data.command_buffer, nullptr);
        ASSERT_NE(frame_data.image_view, nullptr);

        // Wait for the fence to ensure the frame is ready
        frame_data.fence->wait();
        frame_data.fence->reset();

        // Record RHI commands
        frame_data.command_buffer->begin();
        frame_data.command_buffer->transition_image_layout(frame_data.image,
            ImageLayout::Undefined,
            ImageLayout::Present
        );
        frame_data.command_buffer->end();

        // Submit command buffer to the graphics queue
        auto* queue = context->get_graphics_queue();
        queue->submit({frame_data.command_buffer}, frame_data.fence, nullptr);

        // Present the frame
        swapchain->present(frame_data);
    }
}

TEST_F(RHIVulkanTestWithSDLAndSwap, RenderClearColorFrames) {

    constexpr uint32_t frame_count = 4;

    struct SavedFrame {
        rhi::RHISwapchain::RHIFrame frame;
        glm::vec4 clearColor;
    };
    Array<SavedFrame> saved_frames(frame_count);

    // Draw several frames (double buffered)
    for (uint32_t frame_idx = 0; frame_idx < frame_count; ++frame_idx) {
        auto frame_data = swapchain->acquire_next_frame();
        ASSERT_NE(frame_data.command_buffer, nullptr);
        ASSERT_NE(frame_data.image_view, nullptr);

        glm::vec4 clearColor(0.1f * (frame_idx % 10), 0.2f, 0.3f, 1.0f);

        // Wait for the fence to ensure the frame is ready
        frame_data.fence->wait();
        frame_data.fence->reset();

        frame_data.command_buffer->reset();
        frame_data.command_buffer->begin();
        frame_data.command_buffer->transition_image_layout(frame_data.image,
            ImageLayout::Undefined,
            ImageLayout::TransferDst
        );
        frame_data.command_buffer->clear_color(frame_data.image, clearColor);
        frame_data.command_buffer->transition_image_layout(frame_data.image,
            ImageLayout::TransferDst,
            ImageLayout::Present
        );
        frame_data.command_buffer->end();

        auto* queue = context->get_graphics_queue();
        queue->submit({frame_data.command_buffer}, frame_data.fence, nullptr);

        swapchain->present(frame_data);

        saved_frames[frame_idx] = { frame_data, clearColor };
    }

    for (uint32_t frame_idx = 0; frame_idx < saved_frames.size(); ++frame_idx) {
        auto& frame_data = saved_frames[frame_idx].frame;
        auto& clearColor = saved_frames[frame_idx].clearColor;

        // --- Validate clear color using read_image_to_cpu ---
        // Get image size (assuming swapchain exposes width/height or use known values)
        uint32_t width = 640, height = 480;
        std::vector<uint8_t> imageData;
        bool readbackOk = rhi::read_image_to_cpu(context.get(), frame_data.image, width, height, imageData);
        ASSERT_TRUE(readbackOk) << "Failed to read back image data from GPU";
        ASSERT_EQ(imageData.size(), width * height * 4);

        // Convert clearColor to uint8 RGBA
        uint8_t expectedR = static_cast<uint8_t>(std::clamp(clearColor.r, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint8_t expectedG = static_cast<uint8_t>(std::clamp(clearColor.g, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint8_t expectedB = static_cast<uint8_t>(std::clamp(clearColor.b, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint8_t expectedA = static_cast<uint8_t>(std::clamp(clearColor.a, 0.0f, 1.0f) * 255.0f + 0.5f);

        // Check a few sample pixels (corners and center)
        auto check_pixel = [&](uint32_t x, uint32_t y) {
            size_t idx = (y * width + x) * 4;
            ASSERT_EQ(imageData[idx + 0], expectedR) << fmt::format("Pixel ({},{}): R mismatch", x, y);
            ASSERT_EQ(imageData[idx + 1], expectedG) << fmt::format("Pixel ({},{}): G mismatch", x, y);
            ASSERT_EQ(imageData[idx + 2], expectedB) << fmt::format("Pixel ({},{}): B mismatch", x, y);
            ASSERT_EQ(imageData[idx + 3], expectedA) << fmt::format("Pixel ({},{}): A mismatch", x, y);
        };
        check_pixel(0, 0);
        check_pixel(width - 1, 0);
        check_pixel(0, height - 1);
        check_pixel(width - 1, height - 1);
        check_pixel(width / 2, height / 2);
    }
}
