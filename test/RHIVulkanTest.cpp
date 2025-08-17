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
    bool isBGRA = isBGRAFormat(m_swapchain->getFormat());
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
    // Finalized hypothetical API usage: RHIDescriptorBuffer + allocateSet + RHIDescriptorWriter.
    // Assumes forthcoming headers:
    //   rhi/interface/descriptor/RHIDescriptorBuffer.h
    //   rhi/interface/descriptor/RHIDescriptorWriter.h
    // And command buffer extensions:
    //   bindComputePipeline, bindDescriptorBuffers, dispatch

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

    bool bgra = isBGRAFormat(m_swapchain->getFormat());
    StaticArray<uint8,4> expectedGreen = {0,255,0,255};
    expectPixel(imageData, width, height, 0, 0, expectedGreen, bgra, "compute corner TL");
    expectPixel(imageData, width, height, width/2, height/2, expectedGreen, bgra, "compute center");
    expectPixel(imageData, width, height, width-1, height-1, expectedGreen, bgra, "compute corner BR");
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

    struct Vertex { glm::vec4 pos; }; // position (x,y,z)
    const StaticArray<Vertex,3> kVertices = { Vertex{{ 0.0f,  0.6f, 0.0f, 1.0f}},  // top
                                             Vertex{{-0.6f, -0.6f, 0.0f, 1.0f}},  // left
                                             Vertex{{ 0.6f, -0.6f, 0.0f, 1.0f}}}; // right
    // Indices reordered for CCW winding in screen space (required if back-face culling enabled)
    const StaticArray<uint32_t,3> kIndices = {0,2,1};

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
        std::memcpy(vm, kVertices.data(), kVertices.size()*sizeof(Vertex));
        vbuf->unmap();
        void* im = ibuf->map(); ASSERT_NE(im, nullptr);
        std::memcpy(im, kIndices.data(), kIndices.size()*sizeof(uint32_t));
        ibuf->unmap();
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

    // 3. Pipeline layout
    RHIPipelineLayoutBuilder plBuilder(m_ctx.get());
    plBuilder.addDescriptorSetLayout(meshSetLayout.get());
    plBuilder.addPushConstantRange(RHIShaderStage::Mesh | RHIShaderStage::Fragment, 0, sizeof(MeshDrawParamsPC));
    auto pipelineLayout = plBuilder.build();
    ASSERT_NE(pipelineLayout, nullptr);

    // 4. Shader modules (hypothetical): mesh shader consumes TrianglePushConstants to emit triangle
    Path meshShaderPath = "../shaders/colored_triangle.ms.slang.spv";       // Placeholder path (naming TBD)
    Path fragmentShaderPath = "../shaders/colored_triangle.ps.slang.spv";   // Could reuse existing fragment shader variant
    auto meshShader = m_ctx->createShaderModule(meshShaderPath);
    auto fragShader = m_ctx->createShaderModule(fragmentShaderPath);
    ASSERT_NE(meshShader, nullptr);
    ASSERT_NE(fragShader, nullptr);

    // 5. Graphics pipeline descriptor (mesh+fragment)
    RHIGraphicsPipelineDescriptor gpDesc {
        .layout = pipelineLayout.get(),
        .meshShader = meshShader.get(),
        .fragmentShader = fragShader.get(),
        .colorFormat = m_swapchain->getFormat()
    };
    // Hypothetical API call:
    UniquePtr<RHIPipeline> graphicsPipeline = m_ctx->createGraphicsPipeline(gpDesc);
    ASSERT_NE(graphicsPipeline, nullptr);

    // 6. Acquire frame from frame manager
    auto frame = m_frameManager->acquireFrame();
    ASSERT_NE(frame.cmd, nullptr);
    ASSERT_NE(frame.swapImgs.color, nullptr);
    ASSERT_NE(frame.swapImgs.colorView, nullptr);

    // 7. Record commands (implicit cmd->begin() done by FrameManager if that's the pattern)
    // Clear background to black
    frame.cmd->transitionImageLayout(frame.swapImgs.color, RHIImageLayout::TransferDst);
    frame.cmd->clearColor(frame.swapImgs.color, glm::vec4(0,0,0,1));
    frame.cmd->transitionImageLayout(frame.swapImgs.color, RHIImageLayout::ColorAttachment);

    // Begin dynamic rendering once the swap image has been prepped
    m_frameManager->beginDynRendering(frame);

    // Bind pipeline, descriptor buffer & set, then push color
    frame.cmd->bindGraphicsPipeline(graphicsPipeline.get());
    frame.cmd->bindDescriptorBuffers({descBuffer.get()});
    frame.cmd->bindDescriptorSets({
        { .setIndex = 0, .set = meshSet }
    }, pipelineLayout.get(), RHIPipelineBindPoint::Graphics);
    frame.cmd->pushConstants(pipelineLayout.get(), RHIShaderStage::Mesh | RHIShaderStage::Fragment,
        0, sizeof(MeshDrawParamsPC), &params);

    // Dispatch one mesh workgroup: mesh shader uses device addresses to fetch vertices / indices
    frame.cmd->dispatchMesh(1, 1, 1);

    // End dynamic rendering
    m_frameManager->endDynRendering(frame);

    // Prepare for CPU readback
    frame.cmd->transitionImageLayout(frame.swapImgs.color, RHIImageLayout::TransferSrc);
    // Then transition to Present after readback submission (we can transition later again if needed)

    // 8. Submit (choose appropriate pipeline stage masks). We waited on imageAvailable semaphore earlier.
    m_frameManager->submitAndPresent(frame, {
        .queue = m_ctx->getGraphicsQueue(),
        .waitAcquireStage = RHIPipelineStage::ColorAttachmentOutput,
        .signalPresentStage = RHIPipelineStage::ColorAttachmentOutput
    });

    // 9. Read back the image
    uint32 width = frame.swapImgs.color->getWidth();
    uint32 height = frame.swapImgs.color->getHeight();
    Array<uint8> imageData;
    bool readBackOk = RHI::readImageToCpu(m_ctx.get(), frame.swapImgs.color,
        width, height, imageData);
    ASSERT_TRUE(readBackOk);
    ASSERT_EQ(imageData.size(), width * height * 4);

    bool bgra = isBGRAFormat(m_swapchain->getFormat());
    const StaticArray<uint8,4> red   = {255,0,0,255};
    const StaticArray<uint8,4> black = {0,0,0,255};

    // Compute screen-space vertex positions derived from the CPU vertex data (mirrors shader path)
    struct Int2 { int x; int y; };
    auto toScreen = [&](const glm::vec3& p) -> Int2 {
        float sx = (p.x * 0.5f + 0.5f) * static_cast<float>(width);
        float sy = (p.y * 0.5f + 0.5f) * static_cast<float>(height); // Vulkan: origin at top-left
        return Int2{ (int)std::lround(sx), (int)std::lround(sy) };
    };
    Int2 sv0 = toScreen(kVertices[0].pos);
    Int2 sv1 = toScreen(kVertices[1].pos);
    Int2 sv2 = toScreen(kVertices[2].pos);

    // Interior sample points via barycentric combos (weights all positive and sum to 1)
    struct W { float a,b,c; const char* tag; };
    const StaticArray<W,4> baryInside = { W{1.f/3.f, 1.f/3.f, 1.f/3.f, "centroid"},
                                         W{0.55f, 0.25f, 0.20f, "near top"},
                                         W{0.20f, 0.55f, 0.25f, "lower left"},
                                         W{0.25f, 0.20f, 0.55f, "lower right"} };
    for (auto w : baryInside) {
        float fx = w.a*sv0.x + w.b*sv1.x + w.c*sv2.x;
        float fy = w.a*sv0.y + w.b*sv1.y + w.c*sv2.y;
        uint32_t x = (uint32_t)std::clamp<int>((int)std::lround(fx), 0, width-1);
        uint32_t y = (uint32_t)std::clamp<int>((int)std::lround(fy), 0, height-1);
        expectPixel(imageData, width, height, x, y, red, bgra, w.tag);
    }

    // Outside samples: points just beyond each edge normal direction + corners for sanity
    int minX = std::min(std::min(sv0.x, sv1.x), sv2.x);
    int maxX = std::max(std::max(sv0.x, sv1.x), sv2.x);
    int minY = std::min(std::min(sv0.y, sv1.y), sv2.y);
    int maxY = std::max(std::max(sv0.y, sv1.y), sv2.y);
    struct Outside { int x,y; const char* tag; };
    const StaticArray<Outside,7> outsidePts = { Outside{ (minX+maxX)/2, maxY + 12, "below base"},
                                               Outside{ minX - 12, (minY+maxY)/2, "far left"},
                                               Outside{ maxX + 12, (minY+maxY)/2, "far right"},
                                               Outside{ 0,0, "corner TL"},
                                               Outside{ (int)width-1,0, "corner TR"},
                                               Outside{ 0,(int)height-1, "corner BL"},
                                               Outside{ (int)width-1,(int)height-1, "corner BR"} };
    for (auto o : outsidePts) {
        int clampedX = std::clamp(o.x, 0, (int)width-1);
        int clampedY = std::clamp(o.y, 0, (int)height-1);
    expectPixel(imageData, width, height, (uint32)clampedX, (uint32)clampedY, black, bgra, o.tag);
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