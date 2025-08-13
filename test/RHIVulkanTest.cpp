#include <gtest/gtest.h>
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/interface/command/RHICommandBuffer.h"
#include "rhi/interface/queue/RHIQueue.h"
#include "rhi/interface/swapchain/RHISwapchain.h"
#include "rhi/interface/descriptor/RHIDescriptorWriter.h"
#include "rhi/interface/descriptor/RHIDescriptorSetLayoutBuilder.h"
#include "rhi/interface/descriptor/RHIDescriptorSetLayout.h"
#include "rhi/interface/descriptor/RHIDescriptorBuffer.h"
#include "rhi/interface/descriptor/RHIDescriptorSet.h"
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
        m_swapchain = m_context->createSwapchain(m_window, width, height, k_bufferCount,
                                                 RHIImageUsage::TransferSrc |  // So we can read back images
                                                 RHIImageUsage::Storage); // So we can use in compute shaders
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
    // Finalized hypothetical API usage: RHIDescriptorBuffer + allocateSet + RHIDescriptorWriter.
    // Assumes forthcoming headers:
    //   rhi/interface/descriptor/RHIDescriptorBuffer.h
    //   rhi/interface/descriptor/RHIDescriptorWriter.h
    // And command buffer extensions:
    //   bindComputePipeline, bindDescriptorBuffers, dispatch

    constexpr uint32_t kWidth = 640, kHeight = 480;

    auto frame = m_swapchain->acquireNextFrame();
    ASSERT_NE(frame.image, nullptr);
    ASSERT_NE(frame.imageView, nullptr);
    ASSERT_NE(frame.commandBuffer, nullptr);

    frame.fence->wait();
    frame.fence->reset();

    // 1. Descriptor set layout (set 0, binding 0 = storage image)
    auto setLayoutBuilder = m_context->createDescriptorSetLayoutBuilder();
    ASSERT_NE(setLayoutBuilder, nullptr);
    setLayoutBuilder->addBinding(0, RHIDescriptorType::StorageImage);
    auto storageSetLayout = setLayoutBuilder->build(RHIShaderStage::Compute);
    ASSERT_NE(storageSetLayout, nullptr);

    // 2. Pipeline layout
    auto plBuilder = m_context->createPipelineLayoutBuilder();
    ASSERT_NE(plBuilder, nullptr);
    plBuilder->addDescriptorSetLayout(storageSetLayout.get());
    plBuilder->addPushConstantRange(RHIShaderStage::Compute, 0, sizeof(glm::vec4)); // Push constant for clear color
    auto pipelineLayout = plBuilder->build();
    ASSERT_NE(pipelineLayout, nullptr);

    // 3. Compute shader + pipeline
    Path shaderPath = "../shaders/clear.comp.spv";
    auto computeShader = m_context->createShaderModule(shaderPath);
    ASSERT_NE(computeShader, nullptr);
    RHIComputePipelineDescriptor desc {
        .layout = pipelineLayout.get(),
        .computeShader = computeShader.get(),
    };
    UniquePtr<RHIPipeline> computePipeline = m_context->createComputePipeline(desc);
    ASSERT_NE(computePipeline, nullptr);

    // 4. Descriptor buffer and set allocation
    auto buffer = m_context->createDescriptorBuffer({
        .sizeBytes = 128 * 1024
    });
    ASSERT_NE(buffer, nullptr);
    auto set = buffer->allocateSet(storageSetLayout.get(), "ComputeClearSet");
    ASSERT_TRUE(set.isValid());
    set.writeStorageImage(0, 0, frame.imageView).flush();
    
    // 5. Record commands
    frame.commandBuffer->reset();
    frame.commandBuffer->begin();
    frame.commandBuffer->transitionImageLayout(frame.image, RHIImageLayout::General);

    frame.commandBuffer->bindComputePipeline(computePipeline.get());

    frame.commandBuffer->bindDescriptorBuffers({buffer.get()});
    Array<RHIDescriptorSetBinding> setBindings = {
        { .setIndex = 0, .set = set }
    };
    frame.commandBuffer->bindDescriptorSets(setBindings, pipelineLayout.get(),
        RHIPipelineBindPoint::Compute);

    glm::vec4 clearColor(0.0f, 1.0f, 0.0f, 1.0f); // Green clear color
    frame.commandBuffer->pushConstants(pipelineLayout.get(), RHIShaderStage::Compute,
        0, sizeof(clearColor), &clearColor);
    
    uint32_t groupCountX = (kWidth + 7) / 8;
    uint32_t groupCountY = (kHeight + 7) / 8;
    frame.commandBuffer->dispatch(groupCountX, groupCountY, 1);

    frame.commandBuffer->transitionImageLayout(frame.image, RHIImageLayout::Present);
    frame.commandBuffer->end();

    auto* queue = m_context->getGraphicsQueue();
    queue->submit({ frame.commandBuffer }, frame.fence, nullptr);
    frame.fence->wait();
    m_swapchain->present(frame);

    // 6. Readback & validation (green clear (0,255,0,255) expected once shader works)
    std::vector<uint8> imageData;
    bool readBackOk = rhi::readImageToCpu(m_context.get(), frame.image, kWidth, kHeight, imageData);
    ASSERT_TRUE(readBackOk) << "Failed to read back image data from GPU";
    ASSERT_EQ(imageData.size(), kWidth * kHeight * 4);

    auto verifyPixel = [&](uint32_t x, uint32_t y) {
        size_t idx = (y * kWidth + x) * 4;
        bool isBGRA = (m_swapchain->getFormat() == rhi::RHIFormat::RHI_FORMAT_B8G8R8A8_UNORM ||
                        m_swapchain->getFormat() == rhi::RHIFormat::RHI_FORMAT_B8G8R8A8_SRGB);
        if (isBGRA) {
            EXPECT_EQ(imageData[idx + 0], 0u);     // B
            EXPECT_EQ(imageData[idx + 1], 255u);   // G
            EXPECT_EQ(imageData[idx + 2], 0u);     // R
            EXPECT_EQ(imageData[idx + 3], 255u);   // A
        } else {
            EXPECT_EQ(imageData[idx + 0], 0u);     // R
            EXPECT_EQ(imageData[idx + 1], 255u);   // G
            EXPECT_EQ(imageData[idx + 2], 0u);     // B
            EXPECT_EQ(imageData[idx + 3], 255u);   // A
        }
    };
    verifyPixel(0, 0);
    verifyPixel(kWidth/2, kHeight/2);
    verifyPixel(kWidth-1, kHeight-1);
}
