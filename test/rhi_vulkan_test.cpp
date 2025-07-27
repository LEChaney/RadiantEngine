
#include <gtest/gtest.h>
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/rhi_command_buffer.h"
#include "rhi/rhi_queue.h"
#include "rhi/rhi_swapchain.h"
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
    struct SavedFrame {
        rhi::RHISwapchain::RHIFrame frame;
        glm::vec4 clear_color;
    };
    Array<SavedFrame> saved_frames(buffer_count);
    
    // Draw several frames (double buffered)
    constexpr uint32_t frame_count = 100;
    for (uint32_t frame_idx = 0; frame_idx < frame_count; ++frame_idx) {
        auto frame_data = swapchain->acquire_next_frame();
        ASSERT_NE(frame_data.command_buffer, nullptr);
        ASSERT_NE(frame_data.image, nullptr);
        ASSERT_NE(frame_data.image_view, nullptr);

        glm::vec4 clearColor(0.1f * (frame_idx % 10), 0.2f, 0.3f, 1.0f);

        // Wait for the fence to ensure the frame is ready
        frame_data.fence->wait();
        frame_data.fence->reset();

        frame_data.command_buffer->reset();
        frame_data.command_buffer->begin();
        frame_data.command_buffer->transition_image_layout(frame_data.image,
            ImageLayout::TransferDst
        );
        frame_data.command_buffer->clear_color(frame_data.image, clearColor);
        frame_data.command_buffer->transition_image_layout(frame_data.image,
            ImageLayout::Present
        );
        frame_data.command_buffer->end();

        auto* queue = context->get_graphics_queue();
        queue->submit({frame_data.command_buffer}, frame_data.fence, nullptr);

        swapchain->present(frame_data);

        saved_frames[frame_data.image_index % buffer_count] = { frame_data, clearColor };
    }

    bool checked_at_least_one_frame = false;
    // Determine channel order based on swapchain format
    auto format = swapchain->get_format();
    bool isBGRA = (format == rhi::RHIFormat::RHI_FORMAT_B8G8R8A8_UNORM ||
                   format == rhi::RHIFormat::RHI_FORMAT_B8G8R8A8_SRGB);

    for (uint32_t frame_idx = 0; frame_idx < saved_frames.size(); ++frame_idx) {
        auto& frame_data = saved_frames[frame_idx].frame;
        auto& clear_color = saved_frames[frame_idx].clear_color;

        if (!frame_data.image) {
            continue; // Skip if no image was acquired
        }

        // --- Validate clear color using read_image_to_cpu ---
        // Get image size (assuming swapchain exposes width/height or use known values)
        uint32_t width = 640, height = 480;
        std::vector<uint8_t> image_data;
        bool read_back_ok = rhi::read_image_to_cpu(context.get(), frame_data.image, width, height, image_data);
        ASSERT_TRUE(read_back_ok) << "Failed to read back image data from GPU";
        ASSERT_EQ(image_data.size(), width * height * 4);

        // Convert clear_color to uint8 RGBA
        uint8_t expected_r = static_cast<uint8_t>(clear_color.r * 255.0f);
        uint8_t expected_g = static_cast<uint8_t>(clear_color.g * 255.0f);
        uint8_t expected_b = static_cast<uint8_t>(clear_color.b * 255.0f);
        uint8_t expected_a = static_cast<uint8_t>(clear_color.a * 255.0f);

        // Check a few sample pixels (corners and center)
        auto check_pixel = [&](uint32_t x, uint32_t y) {
            size_t idx = (y * width + x) * 4;
            if (isBGRA) {
                EXPECT_EQ(image_data[idx + 0], expected_b) 
                    << fmt::format("Pixel ({},{}): B mismatch (BGRA)", x, y);
                EXPECT_EQ(image_data[idx + 1], expected_g) 
                    << fmt::format("Pixel ({},{}): G mismatch (BGRA)", x, y);
                EXPECT_EQ(image_data[idx + 2], expected_r) 
                    << fmt::format("Pixel ({},{}): R mismatch (BGRA)", x, y);
                EXPECT_EQ(image_data[idx + 3], expected_a) 
                    << fmt::format("Pixel ({},{}): A mismatch (BGRA)", x, y);
            } else {
                EXPECT_EQ(image_data[idx + 0], expected_r) 
                    << fmt::format("Pixel ({},{}): R mismatch (RGBA)", x, y);
                EXPECT_EQ(image_data[idx + 1], expected_g) 
                    << fmt::format("Pixel ({},{}): G mismatch (RGBA)", x, y);
                EXPECT_EQ(image_data[idx + 2], expected_b) 
                    << fmt::format("Pixel ({},{}): B mismatch (RGBA)", x, y);
                EXPECT_EQ(image_data[idx + 3], expected_a) 
                    << fmt::format("Pixel ({},{}): A mismatch (RGBA)", x, y);
            }
        };
        check_pixel(0, 0);
        check_pixel(width - 1, 0);
        check_pixel(0, height - 1);
        check_pixel(width - 1, height - 1);
        check_pixel(width / 2, height / 2);

        checked_at_least_one_frame = true;
    }

    ASSERT_TRUE(checked_at_least_one_frame) << "No valid frames were tested";
}
