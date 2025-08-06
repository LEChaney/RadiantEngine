
#include <gtest/gtest.h>
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/interface/command/RHICommandBuffer.h"
#include "rhi/interface/queue/RHIQueue.h"
#include "rhi/interface/swapchain/RHISwapchain.h"
#include "rhi/interface/descriptor/RHIDescriptorSetLayoutBuilder.h"
#include "rhi/interface/descriptor/RHIDescriptorSetLayout.h"
#include "rhi/interface/pipeline/RHIPipelineLayoutBuilder.h"
#include "rhi/interface/pipeline/RHIPipelineLayout.h"
#include "rhi/interface/pipeline/RHIShaderModule.h"
#include "rhi/interface/pipeline/RHIPipeline.h"
#include "rhi/interface/sync/RHIFence.h"
#include "rhi/interface/image/RHIImageUtils.h"
#include "fmt/format.h"
#include "core/CoreDefs.h"

#include <SDL.h>

#include <algorithm>

using namespace rhi;
using rhi::vulkan::RHIVkContext;

// Helper to capture validation messages
void validationMsgCollector(const char* msg, RHIVkContext::ValidationLevel level) {
    const char* levelStr = "INFO";
    if (level == RHIVkContext::ValidationLevel::Error) {
        levelStr = "ERROR";
    } else if (level == RHIVkContext::ValidationLevel::Warning) {
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
        RHIVkContext::setValidationCallback(validationMsgCollector);
        m_context = RHIVkContext::createUnique(true); // enable validation for test
        ASSERT_NE(m_context, nullptr);
    }

    void TearDown() override {
        RHIVkContext::setValidationCallback(nullptr);
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
        std::vector<uint8> imageData;
        bool readBackOk = rhi::readImageToCpu(m_context.get(), frameData.image, width, height, imageData);
        ASSERT_TRUE(readBackOk) << "Failed to read back image data from GPU";
        ASSERT_EQ(imageData.size(), width * height * 4);

        // Convert clearColor to uint8 RGBA
        uint8 expectedR = static_cast<uint8>(clearColor.r * 255.0f);
        uint8 expectedG = static_cast<uint8>(clearColor.g * 255.0f);
        uint8 expectedB = static_cast<uint8>(clearColor.b * 255.0f);
        uint8 expectedA = static_cast<uint8>(clearColor.a * 255.0f);

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

TEST_F(RHIVulkanTestWithSDLAndSwap, ComputeShaderClearTest) {
    // 1. Create descriptor set layout
    auto setLayoutBuilder = m_context->createDescriptorSetLayoutBuilder();
    setLayoutBuilder->addBinding(0, RHIDescriptorType::StorageImage);
    auto setLayout = setLayoutBuilder->build(RHIShaderStage::Compute);

    // // 2. Create pipeline layout with push constant range
    auto layoutBuilder = m_context->createPipelineLayoutBuilder();
    layoutBuilder->addDescriptorSetLayout(setLayout.get());
    layoutBuilder->addPushConstantRange(RHIShaderStage::Compute, 0, sizeof(glm::vec4));
    auto pipelineLayout = layoutBuilder->build();

    // 3. Create compute shader module
    auto shaderModule = m_context->createShaderModule("../shaders/clear.comp.spv");
    ASSERT_NE(shaderModule, nullptr);

    // 4. Create compute pipeline
    RHIComputePipelineDescriptor pipelineDesc;
    pipelineDesc.layout = pipelineLayout.get();
    pipelineDesc.computeShader = shaderModule.get();
    auto computePipeline = m_context->createComputePipeline(pipelineDesc);
    ASSERT_NE(computePipeline, nullptr);

    // // 5. Acquire swapchain frame
    // auto frame = m_swapchain->acquireNextFrame();
    // ASSERT_NE(frame.commandBuffer, nullptr);
    // ASSERT_NE(frame.image, nullptr);

    // // 6. Create descriptor set using Writer
    // auto writer = m_context->createDescriptorSetWriter(setLayout.get());
    // writer->bindImage(0, frame.image, rhi::RHIDescriptorType::StorageImage);
    // auto descriptorSet = writer->build();

    // // 7. Wait for previous frame
    // frame.fence->wait();
    // frame.fence->reset();

    // // 8. Record compute commands
    // frame.commandBuffer->reset();
    // frame.commandBuffer->begin();

    // frame.commandBuffer->transitionImageLayout(
    //     frame.image, rhi::RHIImageLayout::General
    // );
    // frame.commandBuffer->bindPipeline(computePipeline.get(), rhi::RHIPipelineBindPoint::Compute);
    // frame.commandBuffer->bindDescriptorSet(descriptorSet.get(), rhi::RHIPipelineBindPoint::Compute, pipelineLayout.get());
    // glm::vec4 clearColor(0.1f, 0.6f, 0.2f, 1.0f);
    // frame.commandBuffer->pushConstants(pipelineLayout.get(), RHIShaderStage::Compute, 0, sizeof(clearColor), &clearColor);
    // uint32_t width = 640, height = 480;
    // frame.commandBuffer->dispatchCompute((width + 15) / 16, (height + 15) / 16, 1);
    // frame.commandBuffer->transitionImageLayout(frame.image, rhi::RHIImageLayout::Present);
    // frame.commandBuffer->end();

    // // 9. Submit and present
    // auto* queue = m_context->getGraphicsQueue();
    // queue->submit({frame.commandBuffer}, frame.fence, nullptr);
    // m_swapchain->present(frame);

    // // 10. Read back and verify
    // std::vector<uint8> imageData;
    // bool readBackOk = rhi::readImageToCpu(m_context.get(), frame.image, width, height, imageData);
    // ASSERT_TRUE(readBackOk);
    // ASSERT_EQ(imageData.size(), width * height * 4);

    // auto expectedR = static_cast<uint8>(clearColor.r * 255.0f);
    // auto expectedG = static_cast<uint8>(clearColor.g * 255.0f);
    // auto expectedB = static_cast<uint8>(clearColor.b * 255.0f);
    // auto expectedA = static_cast<uint8>(clearColor.a * 255.0f);

    // auto format = m_swapchain->getFormat();
    // bool isBGRA = (format == rhi::RHIFormat::RHI_FORMAT_B8G8R8A8_UNORM ||
    //                format == rhi::RHIFormat::RHI_FORMAT_B8G8R8A8_SRGB);

    // auto checkPixel = [&](uint32_t x, uint32_t y) {
    //     size_t idx = (y * width + x) * 4;
    //     if (isBGRA) {
    //         EXPECT_EQ(imageData[idx + 0], expectedB);
    //         EXPECT_EQ(imageData[idx + 1], expectedG);
    //         EXPECT_EQ(imageData[idx + 2], expectedR);
    //         EXPECT_EQ(imageData[idx + 3], expectedA);
    //     } else {
    //         EXPECT_EQ(imageData[idx + 0], expectedR);
    //         EXPECT_EQ(imageData[idx + 1], expectedG);
    //         EXPECT_EQ(imageData[idx + 2], expectedB);
    //         EXPECT_EQ(imageData[idx + 3], expectedA);
    //     }
    // };
    // checkPixel(0, 0);
    // checkPixel(width - 1, 0);
    // checkPixel(0, height - 1);
    // checkPixel(width - 1, height - 1);
    // checkPixel(width / 2, height / 2);
}
