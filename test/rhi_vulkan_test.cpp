
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
using rhi::RHIImageLayout;

// Helper to capture validation messages
void validationMsgCollector(const char* msg, RHIVKContext::ValidationLevel level) {
    const char* levelStr = "INFO";
    if (level == RHIVKContext::ValidationLevel::Error) {
        levelStr = "ERROR";
    } else if (level == RHIVKContext::ValidationLevel::Warning) {
        levelStr = "WARNING";
    } else {
        levelStr = "INFO";
    }
    std::string formattedMsg = fmt::format("[Vk Validation][{}] {}", levelStr, msg);
    GTEST_NONFATAL_FAILURE_(formattedMsg.c_str()); // Fail the test on validation errors
}

class RHIVulkanTest : public ::testing::Test {
protected:
    UniquePtr<RHIContext> m_context;

    void SetUp() override {
        RHIVKContext::setValidationCallback(validationMsgCollector);
        m_context = RHIVKContext::createUnique(true); // enable validation for test
        ASSERT_NE(m_context, nullptr);
    }

    void TearDown() override {
        RHIVKContext::setValidationCallback(nullptr);
    }
};

class RHIVulkanTestWithSDLAndSwap : public RHIVulkanTest {
protected:
    SDL_Window* m_window = nullptr;
    UniquePtr<rhi::RHISwapchain> m_swapchain = nullptr;
    const uint32_t k_bufferCount = 2; // double buffering

    void SetUp() override {
        RHIVulkanTest::SetUp();

        // Initialize SDL
        ASSERT_EQ(SDL_Init(SDL_INIT_VIDEO), 0);

        const int width = 640, height = 480;
        m_window = SDL_CreateWindow(
            "RHI Vulkan Swapchain Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
        ASSERT_NE(m_window, nullptr);

        // Create swapchain (API assumed: createSwapchain(SDL_Window*, width, height, buffer_count))
        m_swapchain = m_context->createSwapchain(m_window, width, height, k_bufferCount);
        ASSERT_NE(m_swapchain, nullptr);
        EXPECT_EQ(m_swapchain->imageCount(), k_bufferCount);
    }

    void TearDown() override {
        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        SDL_Quit();
        RHIVulkanTest::TearDown();
    }
};

TEST_F(RHIVulkanTest, BasicInitTeardown) {
}

TEST_F(RHIVulkanTest, CreateContextAndGraphicsQueue) {
    auto* queue = m_context->getGraphicsQueue();
    ASSERT_NE(queue, nullptr);
}

TEST_F(RHIVulkanTest, CreateAndSubmitEmptyCommandBuffer) {
    auto* queue = m_context->getGraphicsQueue();
    ASSERT_NE(queue, nullptr);
    auto cmd = m_context->createCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    cmd->begin();
    cmd->end();
    queue->submit({cmd.get()}, nullptr, nullptr);
    queue->waitIdle();
}

TEST_F(RHIVulkanTestWithSDLAndSwap, RenderSwapchainFrames) {
    // Draw several frames (double buffered)
    constexpr uint32_t k_frameCount = 4;
    for (uint32_t frameIdx = 0; frameIdx < k_frameCount; ++frameIdx) {
        auto frameData = m_swapchain->acquireNextFrame();
        ASSERT_NE(frameData.commandBuffer, nullptr);
        ASSERT_NE(frameData.imageView, nullptr);

        // Wait for the fence to ensure the frame is ready
        frameData.fence->wait();
        frameData.fence->reset();

        // Record RHI commands
        frameData.commandBuffer->begin();
        frameData.commandBuffer->transitionImageLayout(frameData.image,
            RHIImageLayout::Present
        );
        frameData.commandBuffer->end();

        // Submit command buffer to the graphics queue
        auto* queue = m_context->getGraphicsQueue();
        queue->submit({frameData.commandBuffer}, frameData.fence, nullptr);

        // Present the frame
        m_swapchain->present(frameData);
    }
}

TEST_F(RHIVulkanTestWithSDLAndSwap, RenderClearColorFrames) {
    struct SavedFrame {
        rhi::RHISwapchain::RHIFrame frame;
        glm::vec4 clearColor;
    };
    Array<SavedFrame> savedFrames(k_bufferCount);
    
    // Draw several frames (double buffered)
    constexpr uint32_t k_frameCount = 100;
    for (uint32_t frameIdx = 0; frameIdx < k_frameCount; ++frameIdx) {
        auto frameData = m_swapchain->acquireNextFrame();
        ASSERT_NE(frameData.commandBuffer, nullptr);
        ASSERT_NE(frameData.image, nullptr);
        ASSERT_NE(frameData.imageView, nullptr);

        glm::vec4 clearColor(0.1f * (frameIdx % 10), 0.2f, 0.3f, 1.0f);

        // Wait for the fence to ensure the frame is ready
        frameData.fence->wait();
        frameData.fence->reset();

        frameData.commandBuffer->reset();
        frameData.commandBuffer->begin();
        frameData.commandBuffer->transitionImageLayout(frameData.image,
            RHIImageLayout::TransferDst
        );
        frameData.commandBuffer->clearColor(frameData.image, clearColor);
        frameData.commandBuffer->transitionImageLayout(frameData.image,
            RHIImageLayout::Present
        );
        frameData.commandBuffer->end();

        auto* queue = m_context->getGraphicsQueue();
        queue->submit({frameData.commandBuffer}, frameData.fence, nullptr);

        m_swapchain->present(frameData);

        savedFrames[frameData.imageIndex % k_bufferCount] = { frameData, clearColor };
    }

    bool checkedAtLeastOneFrame = false;
    // Determine channel order based on swapchain format
    auto format = m_swapchain->getFormat();
    bool isBGRA = (format == rhi::RHIFormat::RHI_FORMAT_B8G8R8A8_UNORM ||
                   format == rhi::RHIFormat::RHI_FORMAT_B8G8R8A8_SRGB);

    for (uint32_t frameIdx = 0; frameIdx < savedFrames.size(); ++frameIdx) {
        auto& frameData = savedFrames[frameIdx].frame;
        auto& clearColor = savedFrames[frameIdx].clearColor;

        if (!frameData.image) {
            continue; // Skip if no image was acquired
        }

        // --- Validate clear color using read_image_to_cpu ---
        // Get image size (assuming swapchain exposes width/height or use known values)
        uint32_t width = 640, height = 480;
        std::vector<uint8_t> imageData;
        bool readBackOk = rhi::readImageToCpu(m_context.get(), frameData.image, width, height, imageData);
        ASSERT_TRUE(readBackOk) << "Failed to read back image data from GPU";
        ASSERT_EQ(imageData.size(), width * height * 4);

        // Convert clearColor to uint8 RGBA
        uint8_t expectedR = static_cast<uint8_t>(clearColor.r * 255.0f);
        uint8_t expectedG = static_cast<uint8_t>(clearColor.g * 255.0f);
        uint8_t expectedB = static_cast<uint8_t>(clearColor.b * 255.0f);
        uint8_t expectedA = static_cast<uint8_t>(clearColor.a * 255.0f);

        // Check a few sample pixels (corners and center)
        auto checkPixel = [&](uint32_t x, uint32_t y) {
            size_t idx = (y * width + x) * 4;
            if (isBGRA) {
                EXPECT_EQ(imageData[idx + 0], expectedB) 
                    << fmt::format("Pixel ({},{}): B mismatch (BGRA)", x, y);
                EXPECT_EQ(imageData[idx + 1], expectedG) 
                    << fmt::format("Pixel ({},{}): G mismatch (BGRA)", x, y);
                EXPECT_EQ(imageData[idx + 2], expectedR) 
                    << fmt::format("Pixel ({},{}): R mismatch (BGRA)", x, y);
                EXPECT_EQ(imageData[idx + 3], expectedA) 
                    << fmt::format("Pixel ({},{}): A mismatch (BGRA)", x, y);
            } else {
                EXPECT_EQ(imageData[idx + 0], expectedR) 
                    << fmt::format("Pixel ({},{}): R mismatch (RGBA)", x, y);
                EXPECT_EQ(imageData[idx + 1], expectedG) 
                    << fmt::format("Pixel ({},{}): G mismatch (RGBA)", x, y);
                EXPECT_EQ(imageData[idx + 2], expectedB) 
                    << fmt::format("Pixel ({},{}): B mismatch (RGBA)", x, y);
                EXPECT_EQ(imageData[idx + 3], expectedA) 
                    << fmt::format("Pixel ({},{}): A mismatch (RGBA)", x, y);
            }
        };
        checkPixel(0, 0);
        checkPixel(width - 1, 0);
        checkPixel(0, height - 1);
        checkPixel(width - 1, height - 1);
        checkPixel(width / 2, height / 2);

        checkedAtLeastOneFrame = true;
    }

    ASSERT_TRUE(checkedAtLeastOneFrame) << "No valid frames were tested";
}
