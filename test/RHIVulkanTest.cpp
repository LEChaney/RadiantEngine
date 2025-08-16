#include <gtest/gtest.h>
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/interface/command/RHICommandBuffer.h"
#include "rhi/interface/queue/RHIQueue.h"
#include "rhi/interface/swapchain/RHISwapchain.h"
#include "renderer/FrameManager.h"
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
#include "rhi/interface/buffer/RHIBuffer.h" // for vertex/index buffers (device address path)
#include "fmt/format.h"
#include "core/CoreDefs.h"
#include "glm/vec3.hpp"

#include <SDL.h>

#include <algorithm>
#include <string>

using namespace RHI;
using Vulkan::RHIVkContext;

namespace {
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

    if (level == RHIVkContext::ValidationLevel::Error) {
        GTEST_NONFATAL_FAILURE_(formattedMsg.c_str()); // fail only on errors
    } else {
        fmt::println(stderr, "{}", formattedMsg);
    }
}
}

// Parameterized fixture for validation modes
class RHIVulkanTest : public ::testing::TestWithParam<RHIVkContext::ValidationMode> {
protected:
    UniquePtr<RHIContext> m_ctx;

    void SetUp() override {
        RHIVkContext::setValidationCallback(validationMsgCollector);
        auto mode = GetParam();
        m_ctx = RHIVkContext::createUnique(mode);
        ASSERT_NE(m_ctx, nullptr);
    }

    void TearDown() override {
        RHIVkContext::setValidationCallback(nullptr);
        m_ctx.reset();
    }
};

class RHIVulkanTestWithSDLAndSwap : public RHIVulkanTest {
protected:
    SDL_Window* m_window = nullptr;
    UniquePtr<RHI::RHISwapchain> m_swapchain = nullptr;
    UniquePtr<Renderer::FrameManager> m_frameManager = nullptr;
    const uint32 k_bufferCount = 2; // double buffering
    const uint32 k_maxFramesInFlight = 2; // Max CPU run ahead

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
        m_swapchain = m_ctx->createSwapchain(m_window, width, height, k_bufferCount,
                                                 RHIImageUsage::TransferSrc |  // So we can read back images
                                                 RHIImageUsage::Storage); // So we can use in compute shaders
        ASSERT_NE(m_swapchain, nullptr);
        EXPECT_EQ(m_swapchain->imageCount(), k_bufferCount);
        m_frameManager = Renderer::FrameManager::createUnique(m_ctx.get(),
            m_swapchain.get(), k_maxFramesInFlight);
    }

    void TearDown() override {
        // Explicitly destroy in reverse order
        m_frameManager.reset();
        m_swapchain.reset();

        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        SDL_Quit();
        RHIVulkanTest::TearDown();
    }
};

TEST_P(RHIVulkanTest, BasicInitTeardown) {
}

TEST_P(RHIVulkanTest, CreateContextAndGraphicsQueue) {
    auto* queue = m_ctx->getGraphicsQueue();
    ASSERT_NE(queue, nullptr);
}

TEST_P(RHIVulkanTest, CreateAndSubmitEmptyCommandBuffer) {
    auto* queue = m_ctx->getGraphicsQueue();
    ASSERT_NE(queue, nullptr);
    auto cmd = m_ctx->createCommandBuffer();
    ASSERT_NE(cmd, nullptr);
    cmd->begin();
    cmd->end();
    queue->submit({cmd.get()});
    queue->waitIdle();
}

TEST_P(RHIVulkanTestWithSDLAndSwap, RenderSwapchainFrames) {
    // Draw several frames (double buffered)
    constexpr uint32_t k_frameCount = 4;
    for (uint32_t frameIdx = 0; frameIdx < k_frameCount; ++frameIdx) {
        auto frameData = m_frameManager->acquireFrame();
        ASSERT_NE(frameData.cmd, nullptr);
        ASSERT_NE(frameData.swapImgs.color, nullptr);
        ASSERT_NE(frameData.swapImgs.colorView, nullptr);
        ASSERT_NE(frameData.imgAvailableSemaphore, nullptr);
        ASSERT_NE(frameData.renderFinishedSemaphore, nullptr);
        ASSERT_NE(frameData.renderFinishedFence, nullptr);

        // Record RHI commands
        frameData.cmd->transitionImageLayout(frameData.swapImgs.color,
            RHIImageLayout::Present
        );

        // Submit and Present the frame
        m_frameManager->submitAndPresent(frameData);
    }
}

TEST_P(RHIVulkanTestWithSDLAndSwap, RenderClearColorFrames) {
    struct SavedFrame {
        Renderer::FrameContext frame;
        glm::vec4 clearColor;
    };
    Array<SavedFrame> savedFrames(k_bufferCount);
    
    // Draw several frames (double buffered)
    constexpr uint32_t k_frameCount = 100;
    for (uint32_t frameIdx = 0; frameIdx < k_frameCount; ++frameIdx) {
        auto frameData = m_frameManager->acquireFrame();
        ASSERT_NE(frameData.cmd, nullptr);
        ASSERT_NE(frameData.swapImgs.color, nullptr);
        ASSERT_NE(frameData.swapImgs.colorView, nullptr);
        ASSERT_NE(frameData.imgAvailableSemaphore, nullptr);
        ASSERT_NE(frameData.renderFinishedSemaphore, nullptr);
        ASSERT_NE(frameData.renderFinishedFence, nullptr);

        glm::vec4 clearColor(0.1f * (frameIdx % 10), 0.2f, 0.3f, 1.0f);

        frameData.cmd->transitionImageLayout(frameData.swapImgs.color,
            RHIImageLayout::TransferDst
        );
        frameData.cmd->clearColor(frameData.swapImgs.color, clearColor);
        frameData.cmd->transitionImageLayout(frameData.swapImgs.color,
            RHIImageLayout::Present
        );

        m_frameManager->submitAndPresent(frameData, {
            .queue = m_ctx->getGraphicsQueue(),
            .waitAcquireStage = RHIPipelineStage::Transfer,
            .signalPresentStage = RHIPipelineStage::Transfer
        });

        // Note: Indexing by swapchain image index here, since we're reading back data from the
        // swapchain images.
        savedFrames[frameData.swapImgs.imageIndex % k_bufferCount] = { frameData, clearColor };
    }

    bool checkedAtLeastOneFrame = false;
    // Determine channel order based on swapchain format
    auto format = m_swapchain->getFormat();
    bool isBGRA = (format == RHI::RHIFormat::RHI_FORMAT_B8G8R8A8_UNORM ||
                   format == RHI::RHIFormat::RHI_FORMAT_B8G8R8A8_SRGB);

    for (uint32_t frameIdx = 0; frameIdx < savedFrames.size(); ++frameIdx) {
        auto& frameData = savedFrames[frameIdx].frame;
        auto& clearColor = savedFrames[frameIdx].clearColor;

        if (!frameData.swapImgs.color) {
            continue; // Skip if no image was acquired
        }

        // --- Validate clear color using read_image_to_cpu ---
        // Get image size (assuming swapchain exposes width/height or use known values)
        uint32_t width = 640, height = 480;
        std::vector<uint8> imageData;
        bool readBackOk = RHI::readImageToCpu(m_ctx.get(), frameData.swapImgs.color,
            width, height, imageData);
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

TEST_P(RHIVulkanTestWithSDLAndSwap, ComputeShaderClearTest) {
    // Finalized hypothetical API usage: RHIDescriptorBuffer + allocateSet + RHIDescriptorWriter.
    // Assumes forthcoming headers:
    //   rhi/interface/descriptor/RHIDescriptorBuffer.h
    //   rhi/interface/descriptor/RHIDescriptorWriter.h
    // And command buffer extensions:
    //   bindComputePipeline, bindDescriptorBuffers, dispatch

    constexpr uint32_t kWidth = 640, kHeight = 480;

    // 1. Descriptor set layout (set 0, binding 0 = storage image)
    RHIDescriptorSetLayoutBuilder setLayoutBuilder(m_ctx.get());
    setLayoutBuilder.addBinding(0, RHIDescriptorType::StorageImage);
    auto storageSetLayout = setLayoutBuilder.build(RHIShaderStage::Compute);
    ASSERT_NE(storageSetLayout, nullptr);

    // 2. Pipeline layout
    RHIPipelineLayoutBuilder plBuilder(m_ctx.get());
    plBuilder.addDescriptorSetLayout(storageSetLayout.get());
    plBuilder.addPushConstantRange(RHIShaderStage::Compute, 0, sizeof(glm::vec4)); // Push constant for clear color
    auto pipelineLayout = plBuilder.build();
    ASSERT_NE(pipelineLayout, nullptr);

    // 3. Compute shader + pipeline
    Path shaderPath = "../shaders/clear.comp.spv";
    auto computeShader = m_ctx->createShaderModule(shaderPath);
    ASSERT_NE(computeShader, nullptr);
    RHIComputePipelineDescriptor desc {
        .layout = pipelineLayout.get(),
        .computeShader = computeShader.get(),
    };
    UniquePtr<RHIPipeline> computePipeline = m_ctx->createComputePipeline(desc);
    ASSERT_NE(computePipeline, nullptr);

    // 4. Descriptor buffer and set allocation
    auto buffer = m_ctx->createDescriptorBuffer({
        .sizeBytes = 128 * 1024
    });
    ASSERT_NE(buffer, nullptr);
    auto set = buffer->allocateSet(storageSetLayout.get(), "ComputeClearSet");
    ASSERT_TRUE(set.isValid());

    // 5. Record commands
    auto frame = m_frameManager->acquireFrame();
    ASSERT_NE(frame.cmd, nullptr);
    ASSERT_NE(frame.swapImgs.color, nullptr);
    ASSERT_NE(frame.swapImgs.colorView, nullptr);
    ASSERT_NE(frame.imgAvailableSemaphore, nullptr);
    ASSERT_NE(frame.renderFinishedSemaphore, nullptr);
    ASSERT_NE(frame.renderFinishedFence, nullptr);

    // 5.1 Write the swapchain image view to the descriptor
    // We could instead pre-cache a different descriptor set for each swapchain image
    set.writeStorageImage(0, 0, frame.swapImgs.colorView).flush();

    frame.cmd->transitionImageLayout(frame.swapImgs.color, RHIImageLayout::General);

    frame.cmd->bindComputePipeline(computePipeline.get());

    frame.cmd->bindDescriptorBuffers({buffer.get()});
    Array<RHIDescriptorSetBinding> setBindings = {
        { .setIndex = 0, .set = set }
    };
    frame.cmd->bindDescriptorSets(setBindings, pipelineLayout.get(),
        RHIPipelineBindPoint::Compute);

    glm::vec4 clearColor(0.0f, 1.0f, 0.0f, 1.0f); // Green clear color
    frame.cmd->pushConstants(pipelineLayout.get(), RHIShaderStage::Compute,
        0, sizeof(clearColor), &clearColor);
    
    uint32_t groupCountX = (kWidth + 7) / 8;
    uint32_t groupCountY = (kHeight + 7) / 8;
    frame.cmd->dispatch(groupCountX, groupCountY, 1);

    frame.cmd->transitionImageLayout(frame.swapImgs.color, RHIImageLayout::Present);

    m_frameManager->submitAndPresent(frame, {
        .queue = m_ctx->getGraphicsQueue(),
        .waitAcquireStage = RHIPipelineStage::ComputeShader,
        .signalPresentStage = RHIPipelineStage::ComputeShader});

    // 6. Readback & validation (green clear (0,255,0,255) expected once shader works)
    std::vector<uint8> imageData;
    bool readBackOk = RHI::readImageToCpu(m_ctx.get(), frame.swapImgs.color,
        kWidth, kHeight, imageData);
    ASSERT_TRUE(readBackOk) << "Failed to read back image data from GPU";
    ASSERT_EQ(imageData.size(), kWidth * kHeight * 4);

    auto verifyPixel = [&](uint32_t x, uint32_t y) {
        size_t idx = (y * kWidth + x) * 4;
        bool isBGRA = (m_swapchain->getFormat() == RHI::RHIFormat::RHI_FORMAT_B8G8R8A8_UNORM ||
                        m_swapchain->getFormat() == RHI::RHIFormat::RHI_FORMAT_B8G8R8A8_SRGB);
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

TEST_P(RHIVulkanTestWithSDLAndSwap, MeshShaderTriangleRenderTest) {
    // Aspirational test: renders a simple red triangle using a Mesh + Fragment pipeline.
    // Vertex & index buffers are regular GPU buffers with Buffer Device Address (BDA) enabled.
    // Descriptor set (set=0) holds ONLY the 64-bit device addresses (not full storage buffer bindings):
    //   layout(set=0,binding=0) uniform VertexAddress { uint64_t vertexAddr; };
    //   layout(set=0,binding=1) uniform IndexAddress  { uint64_t indexAddr;  };
    // Mesh shader uses those addresses with pointer-style fetches (e.g. via buffer references or
    // manual address arithmetic) to load vertices & indices. Push constants only carry the color.
    // Missing / assumed RHI APIs:
    //   - Descriptor type / write function for raw buffer device address (e.g. writeBufferDeviceAddress)
    //   - RHIBufferUsage::ShaderDeviceAddress + RHIBuffer::getDeviceAddress()
    //   - Mesh shader pipeline creation & dispatchMesh
    //   - Image layout transitions for ColorAttachment / TransferSrc
    //   - Graphics bind point for descriptor sets with device address descriptors
    // Notes:
    //   * Avoids large push constants & avoids binding full storage buffers when only addresses needed.
    //   * Scales to multi-mesh draws by re-writing address descriptors (tiny) or using array elements.

    constexpr uint32_t kWidth = 640, kHeight = 480;
    struct Vertex { glm::vec3 pos; }; // position (x,y,z)
    const Vertex kVertices[3] = {
        {{ 0.0f,  0.6f, 0.0f}}, // top
        {{-0.6f, -0.6f, 0.0f}}, // left
        {{ 0.6f, -0.6f, 0.0f}}, // right
    };
    const uint32_t kIndices[3] = {0,1,2};

    // Create host-visible buffers with device address capability
    auto vbuf = m_ctx->createBuffer(sizeof(kVertices),
        RHIBufferUsage::VertexBuffer | RHIBufferUsage::StorageBuffer | RHIBufferUsage::ShaderDeviceAddress,
        RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent);
    ASSERT_NE(vbuf, nullptr);
    auto ibuf = m_ctx->createBuffer(sizeof(kIndices),
        RHIBufferUsage::IndexBuffer | RHIBufferUsage::StorageBuffer | RHIBufferUsage::ShaderDeviceAddress,
        RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent);
    ASSERT_NE(ibuf, nullptr);

    // Upload data
    {
        void* vm = vbuf->map(); ASSERT_NE(vm, nullptr);
        std::memcpy(vm, kVertices, sizeof(kVertices)); vbuf->unmap();
        void* im = ibuf->map(); ASSERT_NE(im, nullptr);
        std::memcpy(im, kIndices, sizeof(kIndices)); ibuf->unmap();
    }

    // Push constants: only color now
    struct MeshDrawParamsPC {
        glm::vec4 color; // 16B
    } params{};
    params.color = {1,0,0,1};
    static_assert(sizeof(MeshDrawParamsPC) <= 32, "Push constant struct too large");

    // 1. Descriptor set layout (set 0: binding0 vertex device address, binding1 index device address)
    RHIDescriptorSetLayoutBuilder setLayoutBuilder(m_ctx.get());
    setLayoutBuilder
        .addBinding(0, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh)
        .addBinding(1, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh);
    auto meshSetLayout = setLayoutBuilder.build(RHIShaderStage::Mesh);
    ASSERT_NE(meshSetLayout, nullptr);

    // 2. Descriptor buffer + set (write only 64-bit addresses, not entire buffers)
    auto descBuffer = m_ctx->createDescriptorBuffer({ .sizeBytes = 16 * 1024 });
    ASSERT_NE(descBuffer, nullptr);
    auto meshSet = descBuffer->allocateSet(meshSetLayout.get(), "MeshTriangleSet");
    ASSERT_TRUE(meshSet.isValid());
    meshSet.writeStorageBuffer(0, 0, vbuf->createSlice())
           .writeStorageBuffer(1, 0, ibuf->createSlice())
           .flush();

    // // 3. Pipeline layout
    // RHIPipelineLayoutBuilder plBuilder(m_ctx.get());
    // plBuilder.addDescriptorSetLayout(meshSetLayout.get());
    // plBuilder.addPushConstantRange(RHIShaderStage::Mesh | RHIShaderStage::Fragment, 0, sizeof(MeshDrawParamsPC));
    // auto pipelineLayout = plBuilder.build();
    // ASSERT_NE(pipelineLayout, nullptr);
    //
    // // 4. Shader modules (hypothetical): mesh shader consumes TrianglePushConstants to emit triangle
    // Path meshShaderPath = "../shaders/mesh.vert.spv";       // Placeholder path (naming TBD)
    // Path fragmentShaderPath = "../shaders/mesh.frag.spv";   // Could reuse existing fragment shader variant
    // auto meshShader = m_ctx->createShaderModule(meshShaderPath);
    // auto fragShader = m_ctx->createShaderModule(fragmentShaderPath);
    // ASSERT_NE(meshShader, nullptr);
    // ASSERT_NE(fragShader, nullptr);
    //
    // // 5. Graphics pipeline descriptor (mesh+fragment)
    // RHIGraphicsPipelineDescriptor gpDesc {
    //     .layout = pipelineLayout.get(),
    //     .meshShader = meshShader.get(),
    //     .fragmentShader = fragShader.get(),
    // };
    // // Hypothetical API call:
    // UniquePtr<RHIPipeline> graphicsPipeline = m_ctx->createGraphicsPipeline(gpDesc);
    // ASSERT_NE(graphicsPipeline, nullptr);
    //
    // // 6. Acquire frame from frame manager
    // auto frame = m_frameManager->acquireFrame();
    // ASSERT_NE(frame.cmd, nullptr);
    // ASSERT_NE(frame.swapImgs.color, nullptr);
    // ASSERT_NE(frame.swapImgs.colorView, nullptr);
    //
    // // 7. Record commands (implicit cmd->begin() done by FrameManager if that's the pattern)
    // // Clear background to black
    // frame.cmd->transitionImageLayout(frame.swapImgs.color, RHIImageLayout::TransferDst);
    // frame.cmd->clearColor(frame.swapImgs.color, glm::vec4(0,0,0,1));
    // frame.cmd->transitionImageLayout(frame.swapImgs.color, RHIImageLayout::ColorAttachment);
    //
    // // Bind pipeline, descriptor buffer & set, then push color
    // frame.cmd->bindGraphicsPipeline(graphicsPipeline.get());
    // frame.cmd->bindDescriptorBuffers({descBuffer.get()});
    // frame.cmd->bindDescriptorSets({
    //     { .setIndex = 0, .set = meshSet }
    // }, pipelineLayout.get(), RHIPipelineBindPoint::Graphics);
    // frame.cmd->pushConstants(pipelineLayout.get(), RHIShaderStage::Mesh | RHIShaderStage::Fragment,
    //     0, sizeof(MeshDrawParamsPC), &params);
    //
    // // Dispatch one mesh workgroup: mesh shader uses device addresses to fetch vertices / indices
    // frame.cmd->dispatchMesh(1, 1, 1);
    //
    // // Prepare for CPU readback
    // frame.cmd->transitionImageLayout(frame.swapImgs.color, RHIImageLayout::TransferSrc);
    // // Then transition to Present after readback submission (we can transition later again if needed)
    //
    // // 8. Submit (choose appropriate pipeline stage masks). We waited on imageAvailable semaphore earlier.
    // m_frameManager->submitAndPresent(frame, {
    //     .queue = m_ctx->getGraphicsQueue(),
    //     .waitAcquireStage = RHIPipelineStage::ColorAttachmentOutput,
    //     .signalPresentStage = RHIPipelineStage::ColorAttachmentOutput
    // });
    //
    // // 9. Read back the image
    // std::vector<uint8> imageData;
    // bool readBackOk = RHI::readImageToCpu(m_ctx.get(), frame.swapImgs.color,
    //     kWidth, kHeight, imageData);
    // ASSERT_TRUE(readBackOk);
    // ASSERT_EQ(imageData.size(), kWidth * kHeight * 4);
    //
    // bool isBGRA = (m_swapchain->getFormat() == RHI::RHIFormat::RHI_FORMAT_B8G8R8A8_UNORM ||
    //                m_swapchain->getFormat() == RHI::RHIFormat::RHI_FORMAT_B8G8R8A8_SRGB);
    //
    // auto getPixel = [&](uint32_t x, uint32_t y) -> std::array<uint8,4> {
    //     size_t idx = (y * kWidth + x) * 4;
    //     return { imageData[idx+0], imageData[idx+1], imageData[idx+2], imageData[idx+3] };
    // };
    //
    // auto expectColor = [&](const std::array<uint8,4>& px, uint8 r, uint8 g, uint8 b, uint8 a, const char* ctx) {
    //     if (isBGRA) {
    //         EXPECT_EQ(px[0], b) << ctx << " (B)";
    //         EXPECT_EQ(px[1], g) << ctx << " (G)";
    //         EXPECT_EQ(px[2], r) << ctx << " (R)";
    //         EXPECT_EQ(px[3], a) << ctx << " (A)";
    //     } else {
    //         EXPECT_EQ(px[0], r) << ctx << " (R)";
    //         EXPECT_EQ(px[1], g) << ctx << " (G)";
    //         EXPECT_EQ(px[2], b) << ctx << " (B)";
    //         EXPECT_EQ(px[3], a) << ctx << " (A)";
    //     }
    // };
    //
    // // 10. Outside corners should remain black (background)
    // expectColor(getPixel(0,0), 0,0,0,255, "corner TL");
    // expectColor(getPixel(kWidth-1,0), 0,0,0,255, "corner TR");
    // expectColor(getPixel(0,kHeight-1), 0,0,0,255, "corner BL");
    // expectColor(getPixel(kWidth-1,kHeight-1), 0,0,0,255, "corner BR");
    //
    // // 11. Interior sampling: sample several points expected inside the triangle.
    // struct Sample { uint32_t x,y; const char* tag; };
    // Sample inside[] = {
    //     {kWidth/2, kHeight/3,        "inside top"},
    //     {kWidth/2 - 40, kHeight/2,   "inside left"},
    //     {kWidth/2 + 40, kHeight/2,   "inside right"},
    //     {kWidth/2, (2*kHeight)/3 - 25, "inside lower"},
    // };
    // int redCount = 0;
    // for (auto s : inside) {
    //     auto px = getPixel(s.x, s.y);
    //     // Convert to interpreted RGBA for comparison
    //     uint8 r = isBGRA ? px[2] : px[0];
    //     uint8 g = isBGRA ? px[1] : px[1];
    //     uint8 b = isBGRA ? px[0] : px[2];
    //     if (r > 200 && g < 40 && b < 40) {
    //         redCount++;
    //     } else {
    //         ADD_FAILURE() << "Interior sample not red: " << s.tag << " at (" << s.x << "," << s.y << ")";
    //     }
    // }
    // EXPECT_GE(redCount, 2) << "Too few interior red samples (" << redCount << ")";
    //
    // // 12. Just-outside samples beneath/around edges should stay black
    // Sample outside[] = {
    //     {kWidth/2, (2*kHeight)/3 + 10, "below base"},
    //     {kWidth/2 - 90, kHeight/2 + 15, "far left"},
    //     {kWidth/2 + 90, kHeight/2 + 15, "far right"},
    // };
    // for (auto s : outside) {
    //     auto px = getPixel(s.x, s.y);
    //     uint8 r = isBGRA ? px[2] : px[0];
    //     uint8 g = isBGRA ? px[1] : px[1];
    //     uint8 b = isBGRA ? px[0] : px[2];
    //     EXPECT_TRUE(r < 20 && g < 20 && b < 20)
    //         << "Outside sample not black: " << s.tag << " at (" << s.x << "," << s.y << ")";
    // }
}

namespace {
// Instantiate the parameterized tests to run with Standard then GPU-Assisted validation
std::string validationModeToName(
    const ::testing::TestParamInfo<RHIVkContext::ValidationMode>& info) {
    using V = RHIVkContext::ValidationMode;
    switch (info.param) {
        case V::Standard: return "Standard";
        case V::GpuAssisted: return "GpuAssisted";
        default: return "Unknown";
    }
}

INSTANTIATE_TEST_SUITE_P(
    ValidationModes,
    RHIVulkanTest,
    ::testing::Values(
        RHIVkContext::ValidationMode::Standard,
        RHIVkContext::ValidationMode::GpuAssisted),
    validationModeToName);

INSTANTIATE_TEST_SUITE_P(
    ValidationModes,
    RHIVulkanTestWithSDLAndSwap,
    ::testing::Values(
        RHIVkContext::ValidationMode::Standard,
        RHIVkContext::ValidationMode::GpuAssisted),
    validationModeToName);
} // namespace