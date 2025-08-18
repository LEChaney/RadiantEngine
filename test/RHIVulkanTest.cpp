#include <gtest/gtest.h>
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/interface/command/RHICommandBuffer.h"
#include "rhi/interface/queue/RHIQueue.h"
#include "rhi/interface/swapchain/RHISwapchain.h"
#include "renderer/FrameManager.h"
#include "rhi/interface/descriptor/RHIDescriptorWriter.h"
#include "rhi/interface/descriptor/RHIDescriptorSetLayout.h"
#include "rhi/interface/descriptor/RHIDescriptorBuffer.h"
#include "rhi/interface/descriptor/RHIDescriptorSet.h"
#include "rhi/interface/pipeline/RHIPipelineLayout.h"
#include "rhi/interface/pipeline/RHIShaderModule.h"
#include "rhi/interface/pipeline/RHIPipeline.h"
#include "rhi/interface/image/RHIImageUtils.h"
#include "rhi/interface/buffer/RHIBuffer.h" // for vertex/index buffers (device address path)
#include "fmt/format.h"
#include "core/CoreDefs.h"
#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
#include "rhi/interface/image/RHIImage.h"

#include <SDL.h>

#include <algorithm>
#include <string>
#include <array>

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

// --- Shared helpers for readback-based image validation ---
inline bool isBGRAFormat(RHIFormat format) {
    return format == RHIFormat::RHI_FORMAT_B8G8R8A8_UNORM ||
           format == RHIFormat::RHI_FORMAT_B8G8R8A8_SRGB;
}

inline StaticArray<uint8,4> colorToRGBA8(const glm::vec4& c) {
    auto toByte = [](float v) -> uint8 {
        return static_cast<uint8>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
    };
    return { toByte(c.r), toByte(c.g), toByte(c.b), toByte(c.a) };
}

// Validates a single pixel (x,y) against expected RGBA bytes (expected interpreted as RGBA order).
inline void expectPixel(const Array<uint8>& img, uint32 width, uint32 height,
                        uint32 x, uint32 y, const StaticArray<uint8,4>& expected,
                        bool bgra, const char* ctx) {
    ASSERT_LT(x, width);
    ASSERT_LT(y, height);
    size_t idx = (size_t(y) * width + x) * 4;
    if (bgra) {
        EXPECT_EQ(img[idx + 0], expected[2]) << ctx << " (B)"; // B
        EXPECT_EQ(img[idx + 1], expected[1]) << ctx << " (G)"; // G
        EXPECT_EQ(img[idx + 2], expected[0]) << ctx << " (R)"; // R
        EXPECT_EQ(img[idx + 3], expected[3]) << ctx << " (A)"; // A
    } else {
        EXPECT_EQ(img[idx + 0], expected[0]) << ctx << " (R)";
        EXPECT_EQ(img[idx + 1], expected[1]) << ctx << " (G)";
        EXPECT_EQ(img[idx + 2], expected[2]) << ctx << " (B)";
        EXPECT_EQ(img[idx + 3], expected[3]) << ctx << " (A)";
    }
}

inline void expectPixel(const Array<uint8>& img, uint32 width, uint32 height,
                        uint32 x, uint32 y, const glm::vec4& expected,
                        bool bgra, const char* ctx) {
    expectPixel(img, width, height, x, y, colorToRGBA8(expected), bgra, ctx);
}

// --- Mesh shader test helpers (factored out for reuse) ---
// These helpers assume presence of the RHI APIs used in the existing MeshShaderTriangleRenderTest.
// If some calls are not yet implemented, they can be stubbed in the RHI to satisfy the tests later.

struct MeshPipelineResources {
    UniquePtr<RHIDescriptorSetLayout> setLayout;        // set 0 layout
    UniquePtr<RHIPipelineLayout> pipelineLayout;        // pipeline layout
    UniquePtr<RHIDescriptorBuffer> descriptorBuffer;    // backing descriptor buffer
    UniquePtr<RHIShaderModule> meshShader;              // mesh stage shader
    UniquePtr<RHIShaderModule> fragShader;              // fragment stage shader
    UniquePtr<RHIPipeline> graphicsPipeline;            // final graphics pipeline
};

struct MeshTriangleResources {
    UniquePtr<RHIBuffer> vertexBuffer;
    UniquePtr<RHIBuffer> indexBuffer;
    RHIDescriptorSet descriptorSet; // allocated from descriptor buffer
    uint32 indexCount = 0;
    MeshTriangleResources(UniquePtr<RHIBuffer>&& vb,
                          UniquePtr<RHIBuffer>&& ib,
                          RHIDescriptorSet&& set,
                          uint32 count)
        : vertexBuffer(std::move(vb)), indexBuffer(std::move(ib)), descriptorSet(std::move(set)), indexCount(count) {}
};

struct MeshPushConstants {
    glm::vec4 color; // Only color for now
};

MeshPipelineResources createMeshPipelineForColorTriangles(RHIContext* ctx, RHIFormat colorFormat,
                                                          RHIFormat depthFormat,
                                                          const Path& meshShaderPath,
                                                          const Path& fragShaderPath) {
    MeshPipelineResources out{};

    // Descriptor set layout (set0: binding0 vertex buffer addr, binding1 index buffer addr)
    out.setLayout = ctx->createDescriptorSetLayout({
        { 0, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh },
        { 1, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh }
    });

    // Descriptor buffer
    out.descriptorBuffer = ctx->createDescriptorBuffer({ .sizeBytes = 32 * 1024 });

    // Pipeline layout
    out.pipelineLayout = ctx->createPipelineLayout({
        .setLayouts = { out.setLayout.get() },
        .pushConstantRanges = {{
            .stages = RHIShaderStage::Mesh | RHIShaderStage::Fragment,
            .offset = 0,
            .size = sizeof(MeshPushConstants)
        }}
    });

    // Shaders
    out.meshShader = ctx->createShaderModule(meshShaderPath);
    out.fragShader = ctx->createShaderModule(fragShaderPath);

    // Graphics pipeline (mesh + fragment)
    RHIGraphicsPipelineDescriptor gpDesc {
        .layout = out.pipelineLayout.get(),
        .meshShader = out.meshShader.get(),
        .fragmentShader = out.fragShader.get(),
        .colorFormat = colorFormat,
        .depthFormat = depthFormat
    };
    out.graphicsPipeline = ctx->createGraphicsPipeline(gpDesc);
    return out;
}

MeshTriangleResources createMeshTriangleResources(RHIContext* ctx, MeshPipelineResources& pipe,
                                                  const Array<glm::vec4>& positions, // vec4 pos
                                                  const Array<uint32_t>& indices) {
    const size_t vSize = positions.size() * sizeof(glm::vec4);
    auto vbuf = ctx->createBuffer(vSize,
        RHIBufferUsage::VertexBuffer | RHIBufferUsage::StorageBuffer | RHIBufferUsage::ShaderDeviceAddress,
        RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent);
    void* vm = vbuf->map();
    std::memcpy(vm, positions.data(), vSize);
    vbuf->unmap();

    const size_t iSize = indices.size() * sizeof(uint32_t);
    auto ibuf = ctx->createBuffer(iSize,
        RHIBufferUsage::IndexBuffer | RHIBufferUsage::StorageBuffer | RHIBufferUsage::ShaderDeviceAddress,
        RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent);
    void* im = ibuf->map();
    std::memcpy(im, indices.data(), iSize);
    ibuf->unmap();
    uint32 count = (uint32)indices.size();

    auto set = pipe.descriptorBuffer->allocateSet(pipe.setLayout.get(), "MeshTriangleSet");
    set
        .writeStorageBuffer(0, 0, vbuf->createSlice())
        .writeStorageBuffer(1, 0, ibuf->createSlice())
        .flush();

    return MeshTriangleResources(std::move(vbuf), std::move(ibuf), std::move(set), count);
}

void recordDrawMeshTriangle(RHICommandBuffer* cmd, const MeshPipelineResources& pipe,
                            const MeshTriangleResources& tri, const glm::vec4& color) {
    MeshPushConstants pc{ color };
    cmd->bindGraphicsPipeline(pipe.graphicsPipeline.get());
    cmd->bindDescriptorBuffers({ pipe.descriptorBuffer.get() });
    cmd->bindDescriptorSets({ { .setIndex = 0, .set = tri.descriptorSet } },
        pipe.pipelineLayout.get(), RHIPipelineBindPoint::Graphics);
    cmd->pushConstants(pipe.pipelineLayout.get(), RHIShaderStage::Mesh | RHIShaderStage::Fragment,
        0, sizeof(MeshPushConstants), &pc);
    // Mesh shader path uses dispatchMesh(1,1,1) to emit primitives based on buffers.
    cmd->dispatchMesh(1,1,1);
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
        m_swapchain = m_ctx->createSwapchain({
            .window = m_window,
            .width = width,
            .height = height,
            .imageCount = k_bufferCount,
            .depthFormat = RHIFormat::RHI_FORMAT_D32_SFLOAT,
            .extraColorUsage = RHIImageUsage::TransferSrc // For readback command
                | RHIImageUsage::TransferDst // For clear command
                | RHIImageUsage::Storage     // For compute shader usage
        });
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
    bool isBGRA = isBGRAFormat(m_swapchain->getColorFormat());
    for (auto& [frame, clearColor] : savedFrames) {
        auto& frameData = frame;
        if (!frameData.swapImgs.color) {
            continue;
        }

        uint32 width = frameData.swapImgs.color->getWidth();
        uint32 height = frameData.swapImgs.color->getHeight();
        Array<uint8> imageData;
        ASSERT_TRUE(RHI::readImageToCpu(m_ctx.get(), frameData.swapImgs.color, width, height, imageData));
        ASSERT_EQ(imageData.size(), width * height * 4);

        auto expected = colorToRGBA8(clearColor);
        expectPixel(imageData, width, height, 0, 0, expected, isBGRA, "clear corner TL");
        expectPixel(imageData, width, height, width-1, 0, expected, isBGRA, "clear corner TR");
        expectPixel(imageData, width, height, 0, height-1, expected, isBGRA, "clear corner BL");
        expectPixel(imageData, width, height, width-1, height-1, expected, isBGRA, "clear corner BR");
        expectPixel(imageData, width, height, width/2, height/2, expected, isBGRA, "clear center");
        checkedAtLeastOneFrame = true;
    }
    ASSERT_TRUE(checkedAtLeastOneFrame) << "No valid frames were tested";
}

TEST_P(RHIVulkanTestWithSDLAndSwap, ComputeShaderClearTest) {
    // 1. Descriptor set layout (set 0, binding 0 = storage image)
    auto storageSetLayout = m_ctx->createDescriptorSetLayout({
        { 0, RHIDescriptorType::StorageImage, RHIShaderStage::Compute }
    });
    ASSERT_NE(storageSetLayout, nullptr);

    // 2. Pipeline layout
    auto pipelineLayout = m_ctx->createPipelineLayout({
        .setLayouts = { storageSetLayout.get() },
        .pushConstantRanges = { {
            .stages = RHIShaderStage::Compute,
            .offset = 0,
            .size = sizeof(glm::vec4)
        } }
    });
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

    uint32 width = frame.swapImgs.color->getWidth();
    uint32 height = frame.swapImgs.color->getHeight();
    uint32_t groupCountX = (width + 7) / 8;
    uint32_t groupCountY = (height + 7) / 8;
    frame.cmd->dispatchCompute(groupCountX, groupCountY, 1);

    m_frameManager->submitAndPresent(frame, {
        .queue = m_ctx->getGraphicsQueue(),
        .waitAcquireStage = RHIPipelineStage::ComputeShader,
        .signalPresentStage = RHIPipelineStage::ComputeShader});

    // 6. Readback & validation (green clear (0,255,0,255) expected once shader works)
    Array<uint8> imageData;
    bool readBackOk = RHI::readImageToCpu(m_ctx.get(), frame.swapImgs.color,
        width, height, imageData);
    ASSERT_TRUE(readBackOk) << "Failed to read back image data from GPU";
    ASSERT_EQ(imageData.size(), width * height * 4);

    bool bgra = isBGRAFormat(m_swapchain->getColorFormat());
    StaticArray<uint8,4> expectedGreen = {0,255,0,255};
    expectPixel(imageData, width, height, 0, 0, expectedGreen, bgra, "compute corner TL");
    expectPixel(imageData, width, height, width/2, height/2, expectedGreen, bgra, "compute center");
    expectPixel(imageData, width, height, width-1, height-1, expectedGreen, bgra, "compute corner BR");
}

TEST_P(RHIVulkanTestWithSDLAndSwap, MeshShaderTriangleRenderTest) {
    // Triangle geometry (vec4 positions)
    Array<glm::vec4> positions = {
        { 0.0f,  0.6f, 0.0f, 1.0f},  // top
        {-0.6f, -0.6f, 0.0f, 1.0f},  // left
        { 0.6f, -0.6f, 0.0f, 1.0f}   // right
    };
    Array<uint32_t> indices = {0,2,1}; // CCW

    Path meshShaderPath = "../shaders/colored_triangle.ms.slang.spv";
    Path fragmentShaderPath = "../shaders/colored_triangle.ps.slang.spv";
    auto pipelineRes = createMeshPipelineForColorTriangles(m_ctx.get(),
        m_swapchain->getColorFormat(), m_swapchain->getDepthFormat(),
        meshShaderPath, fragmentShaderPath);
    ASSERT_NE(pipelineRes.graphicsPipeline, nullptr);

    auto triRes = createMeshTriangleResources(m_ctx.get(), pipelineRes, positions, indices);
    ASSERT_TRUE(triRes.descriptorSet.isValid());

    // Acquire frame
    auto frame = m_frameManager->acquireFrame();
    ASSERT_NE(frame.cmd, nullptr);
    m_frameManager->beginDynRendering(frame);
    recordDrawMeshTriangle(frame.cmd, pipelineRes, triRes, {1,0,0,1});
    m_frameManager->endDynRendering(frame);
    m_frameManager->submitAndPresent(frame, {
        .queue = m_ctx->getGraphicsQueue(),
        .waitAcquireStage = RHIPipelineStage::ColorAttachmentOutput,
        .signalPresentStage = RHIPipelineStage::ColorAttachmentOutput });

    // Readback & validate a few pixels roughly inside triangle & outside
    uint32 width = frame.swapImgs.color->getWidth();
    uint32 height = frame.swapImgs.color->getHeight();
    Array<uint8> imageData; ASSERT_TRUE(RHI::readImageToCpu(m_ctx.get(), frame.swapImgs.color, width, height, imageData));
    ASSERT_EQ(imageData.size(), width*height*4);
    bool bgra = isBGRAFormat(m_swapchain->getColorFormat());
    const StaticArray<uint8,4> red   = {255,0,0,255};
    const StaticArray<uint8,4> black = {0,0,0,255};
    auto toScreen = [&](const glm::vec2& p) {
        uint32 x = (uint32)std::clamp<int>((int)std::lround((p.x*0.5f+0.5f)*width),0,(int)width-1);
        uint32 y = (uint32)std::clamp<int>((int)std::lround((p.y*0.5f+0.5f)*height),0,(int)height-1);
        return std::pair<uint32,uint32>{x,y}; };
    for (glm::vec2 sample : { glm::vec2{0.f,0.f}, glm::vec2{-0.2f,-0.2f}, glm::vec2{0.2f,-0.2f} }) {
        auto [sx,sy] = toScreen(sample);
        expectPixel(imageData,width,height,sx,sy,red,bgra,"triangle inside"); }
    for (glm::vec2 sample : { glm::vec2{-0.9f,-0.9f}, glm::vec2{0.9f,0.9f} }) {
        auto [sx,sy] = toScreen(sample);
        expectPixel(imageData,width,height,sx,sy,black,bgra,"triangle outside"); }
}

TEST_P(RHIVulkanTestWithSDLAndSwap, MeshShaderDepthTwoTrianglesTest) {
    // Render two triangles overlapping in screen space with different depths and verify depth test keeps nearer triangle.
    // Geometry: near (red) drawn FIRST, far (green) drawn SECOND. Correct depth test => overlap shows red.
    Array<glm::vec4> nearPositions = {
        {-0.6f, -0.4f, 0.8f, 1.0f},
        { 0.0f,  0.6f, 0.8f, 1.0f},
        { 0.6f, -0.4f, 0.8f, 1.0f}
    };
    Array<glm::vec4> farPositions = {
        {-0.2f, -0.4f, 0.2f, 1.0f},
        { 0.4f,  0.6f, 0.2f, 1.0f},
        { 1.0f, -0.4f, 0.2f, 1.0f}
    };
    Array<uint32_t> indices = {0,2,1}; // CCW

    Path meshShaderPath = "../shaders/colored_triangle.ms.slang.spv";
    Path fragmentShaderPath = "../shaders/colored_triangle.ps.slang.spv";
    auto pipelineRes = createMeshPipelineForColorTriangles(m_ctx.get(),
        m_swapchain->getColorFormat(),
        m_swapchain->getDepthFormat(),
        meshShaderPath, fragmentShaderPath);
    ASSERT_NE(pipelineRes.graphicsPipeline, nullptr);

    auto nearTri = createMeshTriangleResources(m_ctx.get(), pipelineRes, nearPositions, indices);
    auto farTri  = createMeshTriangleResources(m_ctx.get(), pipelineRes, farPositions, indices);
    ASSERT_TRUE(nearTri.descriptorSet.isValid());
    ASSERT_TRUE(farTri.descriptorSet.isValid());

    auto frame = m_frameManager->acquireFrame();
    ASSERT_NE(frame.cmd, nullptr);
    m_frameManager->beginDynRendering(frame); // Assumes default depth attachment with depth test/write enabled.
    // Draw near first (red)
    recordDrawMeshTriangle(frame.cmd, pipelineRes, nearTri, {1,0,0,1});
    // Draw far second (green) - should be occluded where overlapping
    recordDrawMeshTriangle(frame.cmd, pipelineRes, farTri, {0,1,0,1});
    m_frameManager->endDynRendering(frame);
    frame.cmd->transitionImageLayout(frame.swapImgs.color, RHIImageLayout::TransferSrc);
    m_frameManager->submitAndPresent(frame, {
        .queue = m_ctx->getGraphicsQueue(),
        .waitAcquireStage = RHIPipelineStage::ColorAttachmentOutput,
        .signalPresentStage = RHIPipelineStage::ColorAttachmentOutput });

    uint32 width = frame.swapImgs.color->getWidth();
    uint32 height = frame.swapImgs.color->getHeight();
    Array<uint8> imageData; ASSERT_TRUE(RHI::readImageToCpu(m_ctx.get(), frame.swapImgs.color, width, height, imageData));
    ASSERT_EQ(imageData.size(), width*height*4);
    bool bgra = isBGRAFormat(m_swapchain->getColorFormat());
    const StaticArray<uint8,4> red   = {255,0,0,255};
    const StaticArray<uint8,4> green = {0,255,0,255};
    const StaticArray<uint8,4> black = {0,0,0,255};

    auto ndcToPixel = [&](float nx, float ny){
        uint32 x = (uint32)std::clamp<int>((int)std::lround((nx*0.5f+0.5f)*width),0,(int)width-1);
        uint32 y = (uint32)std::clamp<int>((int)std::lround((ny*0.5f+0.5f)*height),0,(int)height-1);
        return std::pair<uint32,uint32>{x,y}; };

    // Sample inside near-only region (-0.5,0)
    {
        auto [x,y] = ndcToPixel(-0.1f, 0.0f);
        expectPixel(imageData,width,height,x,y,red,bgra,"near-only region");
    }
    // Sample inside far-only region (0.85,0) expected green
    {
        auto [x,y] = ndcToPixel(0.75f, 0.0f);
        expectPixel(imageData,width,height,x,y,green,bgra,"far-only region");
    }
    // Sample overlapping region (0.2,0) should be red (near triangle not overwritten by far draw)
    {
        auto [x,y] = ndcToPixel(0.3f, 0.0f);
        expectPixel(imageData,width,height,x,y,red,bgra,"overlap region depth");
    }
    // Outside both (-0.9,-0.9) should be background black
    {
        auto [x,y] = ndcToPixel(-0.9f, -0.9f);
        expectPixel(imageData,width,height,x,y,black,bgra,"outside region");
    }
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