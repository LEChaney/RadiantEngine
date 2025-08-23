#include "rhi/vulkan/core/RHIVkContext.h"
#include "renderer/FrameManager.h"
#include "resource/mesh/generator/MeshPrimitives.h"
#include "resource/mesh/meshlet/MeshletBuilder.h"
#include "rhi/interface/descriptor/RHIDescriptorSetLayout.h"
#include "rhi/interface/descriptor/RHIDescriptorBuffer.h"
#include "rhi/interface/descriptor/RHIDescriptorSet.h"
#include "rhi/interface/pipeline/RHIPipelineLayout.h"
#include "rhi/interface/pipeline/RHIPipeline.h"
#include "rhi/interface/pipeline/RHIShaderModule.h"
#include "rhi/interface/buffer/RHIBuffer.h"
#include "rhi/interface/swapchain/RHISwapchain.h"
#include "rhi/interface/image/RHIImage.h"
#include "rhi/interface/image/RHIImageView.h"
#include "rhi/interface/queue/RHIQueue.h"
#include "rhi/interface/command/RHICommandBuffer.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <cstring>

using namespace RHI;
using namespace resource::mesh::primitives;
using namespace resource::mesh::meshlet;
using Renderer::FrameManager;

struct MeshletPipelineResources {
    UniquePtr<RHIDescriptorSetLayout> setLayout;
    UniquePtr<RHIPipelineLayout> pipelineLayout;
    UniquePtr<RHIDescriptorBuffer> descriptorBuffer;
    UniquePtr<RHIShaderModule> meshShader;
    UniquePtr<RHIShaderModule> fragShader;
    UniquePtr<RHIPipeline> graphicsPipeline;
};

namespace {
MeshletPipelineResources createMeshletPipeline(RHIContext* ctx, RHIFormat colorFmt, RHIFormat depthFmt,
                                               const Path& msPath, const Path& psPath){
    MeshletPipelineResources out{};
    out.setLayout = ctx->createDescriptorSetLayout({
        {0, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh}, // meshlets
        {1, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh}, // packed vertices
        {2, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh}  // primitive local indices
    });
    out.descriptorBuffer = ctx->createDescriptorBuffer({ .sizeBytes = 64 * 1024 });
    out.pipelineLayout = ctx->createPipelineLayout({
        .setLayouts = { out.setLayout.get() },
        .pushConstantRanges = { { .stages = RHIShaderStage::Mesh, .offset = 0, .size = sizeof(glm::mat4) } }
    });
    out.meshShader = ctx->createShaderModule(msPath);
    out.fragShader = ctx->createShaderModule(psPath);
    RHIGraphicsPipelineDescriptor desc{
        .layout = out.pipelineLayout.get(),
        .meshShader = out.meshShader.get(),
        .fragmentShader = out.fragShader.get(),
        .colorFormat = colorFmt,
        .depthFormat = depthFmt
    };
    out.graphicsPipeline = ctx->createGraphicsPipeline(desc);
    return out;
}

glm::mat4 makePerspectiveReverseZ(float fovyRadians, float aspect, float zNear, float zFar){
    float f = 1.f / std::tan(fovyRadians * 0.5f);
    float A = zNear / (zFar - zNear);
    float B = (zFar * zNear) / (zFar - zNear);
    glm::mat4 m(0.0f);
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = A;
    m[2][3] = -1.f;
    m[3][2] = B;
    return m;
}
}

int main(int argc, char** argv){
    (void)argc; (void)argv;
    if(SDL_Init(SDL_INIT_VIDEO) != 0){
        std::fprintf(stderr,"SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const int width = 1280, height = 720;
    SDL_Window* window = SDL_CreateWindow(
        "RadiantEngine Axes Gizmo",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
    );
    if(!window){
        std::fprintf(stderr,"SDL window failed: %s\n", SDL_GetError());
        return 1;
    }

    auto ctx = Vulkan::RHIVkContext::createUnique(Vulkan::RHIVkContext::ValidationMode::Auto);
    auto swapchain = ctx->createSwapchain({
        .window = window,
        .width = (uint32)width,
        .height = (uint32)height,
        .imageCount = 2,
        .depthFormat = RHIFormat::RHI_FORMAT_D32_SFLOAT,
        .extraColorUsage = RHIImageUsage::TransferDst
    });
    auto frameMgr = FrameManager::createUnique(ctx.get(), swapchain.get(), 2);

    // Build axes gizmo mesh and meshlets
    AxesDesc adesc {
        .axisLength = 1.5f,
        .shaftRadius = 0.02f,
        .coneRadius = 0.08f,
        .coneHeight = 0.2f,
        .radialSegments = 24
    };
    MeshSrcData gizmo = MakeAxesSrcData(adesc);
    BuildParams buildParams; buildParams.gridResolution = 0; // sequential meshlets
    MeshletBuildResult build = buildMeshlets(gizmo, buildParams);
    PackedMeshlets packed = packMeshlets(gizmo, build);

    auto makeBuf = [&](const void* data, size_t sz, RHIBufferUsageFlags usage = 0) {
        auto buf = ctx->createBuffer(
            sz,
            usage | RHIBufferUsage::StorageBuffer | RHIBufferUsage::ShaderDeviceAddress,
            RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent
        );
        void* m = buf->map();
        std::memcpy(m,data,sz);
        buf->unmap();
        return buf;
    };
    auto meshletsBuf = makeBuf(packed.meshlets.data(), packed.meshlets.size()*sizeof(Meshlet));
    auto verticesBuf = makeBuf(packed.vertices.data(), packed.vertices.size()*sizeof(Vertex));
    auto primIdxBuf  = makeBuf(packed.indices.data(), packed.indices.size()*sizeof(uint8));

    auto pipeline = createMeshletPipeline(
        ctx.get(), swapchain->getColorFormat(), swapchain->getDepthFormat(),
        "shaders/mesh_meshlet.ms.slang.spv", "shaders/mesh_meshlet.ps.slang.spv");

    auto set = pipeline.descriptorBuffer->allocateSet(pipeline.setLayout.get(), "AxesSet");
    set .writeStorageBuffer(0,0, meshletsBuf->createSlice())
        .writeStorageBuffer(1,0, verticesBuf->createSlice())
        .writeStorageBuffer(2,0, primIdxBuf->createSlice())
        .flush();

    bool running = true; uint64 startTicks = SDL_GetTicks64();
    while(running){
        SDL_Event e; while(SDL_PollEvent(&e)){ if(e.type == SDL_QUIT) running=false; }
        float timeSec = float(SDL_GetTicks64() - startTicks) / 1000.f;
        auto frame = frameMgr->acquireFrame();
        frameMgr->beginDynRendering(frame);
        // Camera: slight orbit to show 3D
        glm::mat4 proj = makePerspectiveReverseZ(glm::radians(60.f), float(width)/float(height), 0.01f, 50.f);
        proj[1][1] *= -1.f;
        glm::vec3 eye = { 0.15f, 1.0f, 4.0f };
        glm::mat4 view = glm::lookAt(eye, glm::vec3(0), glm::vec3(0,1,0));
        glm::mat4 model = glm::mat4(1.f);
        glm::mat4 mvp = proj * view * model;

        frame.cmd->bindGraphicsPipeline(pipeline.graphicsPipeline.get());
        frame.cmd->bindDescriptorBuffers({ pipeline.descriptorBuffer.get() });
        frame.cmd->bindDescriptorSets({ { .setIndex = 0, .set = set } },
            pipeline.pipelineLayout.get(), RHIPipelineBindPoint::Graphics);
        frame.cmd->pushConstants(pipeline.pipelineLayout.get(), RHIShaderStage::Mesh, 0, sizeof(glm::mat4), &mvp);
        frame.cmd->dispatchMesh((uint32)packed.meshlets.size(), 1, 1);
        frameMgr->endDynRendering(frame);
        frameMgr->submitAndPresent(frame, {});
    }
    ctx->getGraphicsQueue()->waitIdle();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
