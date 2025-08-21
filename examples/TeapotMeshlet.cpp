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
    out.descriptorBuffer = ctx->createDescriptorBuffer({ .sizeBytes = 128 * 1024 });
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
    // Reversed-Z (near -> depth 1, far -> depth 0), Vulkan zero-to-one clip
    // Column-major indexing: m[col][row]
    float A = zNear / (zFar - zNear);              // near/(far-near)
    float B = (zFar * zNear) / (zFar - zNear);     // (far*near)/(far-near)
    glm::mat4 m(0.0f);
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = A;    // col2,row2
    m[2][3] = -1.f; // col2,row3
    m[3][2] = B;    // col3,row2
    return m;       // m[3][3] = 0
}
}

int main(int argc, char** argv){
    (void)argc; (void)argv;
    // Init SDL & window
    if(SDL_Init(SDL_INIT_VIDEO) != 0){
        std::fprintf(stderr,"SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    const int width = 2560/2, height = 1600/2;
    SDL_Window* window = SDL_CreateWindow(
        "RadiantEngine Teapot",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
    );
    if(!window){
        std::fprintf(stderr,"SDL window failed: %s\n", SDL_GetError());
        return 1;
    }

    // Create Vulkan RHI context (standard validation)
    auto ctx = Vulkan::RHIVkContext::createUnique(Vulkan::RHIVkContext::ValidationMode::Standard);

    auto swapchain = ctx->createSwapchain({
        .window = window,
        .width = (uint32)width,
        .height = (uint32)height,
        .imageCount = 2,
        .depthFormat = RHIFormat::RHI_FORMAT_D32_SFLOAT,
        .extraColorUsage = RHIImageUsage::TransferDst
    });

    auto frameMgr = FrameManager::createUnique(ctx.get(), swapchain.get(), 2);

    // Generate teapot mesh source (medium tessellation)
    TeapotDesc tdesc; tdesc.tessellation = 10; tdesc.scale = 0.15f; tdesc.color = {1,0.8f,0.6f,1};
    MeshSrcData teapot = MakeTeapotSrcData(tdesc);

    // Build meshlets
    BuildParams buildParams; buildParams.gridResolution = 0; // simple order
    MeshletBuildResult build = buildMeshlets(teapot, buildParams);
    PackedMeshlets packed = packMeshlets(teapot, build);

    // Create GPU buffers for meshlet data
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

    // Pipeline
    // Executable placed in bin/(Config)/ ; shaders copied to bin/(Config)/shaders
    auto pipeline = createMeshletPipeline(ctx.get(), swapchain->getColorFormat(), swapchain->getDepthFormat(),
        "shaders/mesh_meshlet.ms.slang.spv", "shaders/mesh_meshlet.ps.slang.spv");

    auto set = pipeline.descriptorBuffer->allocateSet(pipeline.setLayout.get(), "MeshletSet");
    set .writeStorageBuffer(0,0, meshletsBuf->createSlice())
        .writeStorageBuffer(1,0, verticesBuf->createSlice())
        .writeStorageBuffer(2,0, primIdxBuf->createSlice())
        .flush();

    bool running = true; uint64 startTicks = SDL_GetTicks64();
    while(running){
        SDL_Event e; while(SDL_PollEvent(&e)){ if(e.type == SDL_QUIT) running=false; }
        float timeSec = float(SDL_GetTicks64() - startTicks) / 1000.f;
        // Acquire frame
        auto frame = frameMgr->acquireFrame();
        frameMgr->beginDynRendering(frame);

        // Build MVP (closer orbit for a more filled frame)
        glm::mat4 proj = makePerspectiveReverseZ(glm::radians(55.f), float(width)/float(height), 0.01f, 50.f);
        proj[1][1] *= -1.f; // GLM depth zero to one + invert Y for Vulkan style? (if needed)
        const float orbitRadius = 0.65f;
        const float orbitHeight = 0.18f;
        const glm::vec3 target = {0.f, 0.0f, 0.f};
        glm::vec3 eye = glm::vec3( std::sin(timeSec*0.7f)*orbitRadius, orbitHeight, std::cos(timeSec*0.7f)*orbitRadius + 0.15f);
        glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0,1,0));
        glm::mat4 model = glm::rotate(glm::mat4(1.f), timeSec*0.8f, glm::vec3(0,1,0));
        glm::mat4 mvp = proj * view * model;

        frame.cmd->bindGraphicsPipeline(pipeline.graphicsPipeline.get());
        frame.cmd->bindDescriptorBuffers({ pipeline.descriptorBuffer.get() });
        frame.cmd->bindDescriptorSets({ { .setIndex = 0, .set = set } },
            pipeline.pipelineLayout.get(), RHIPipelineBindPoint::Graphics);
        frame.cmd->pushConstants(pipeline.pipelineLayout.get(),
            RHIShaderStage::Mesh, 0, sizeof(glm::mat4), &mvp);
        uint32 meshletCount = (uint32)packed.meshlets.size();
        frame.cmd->dispatchMesh(meshletCount, 1, 1);

        frameMgr->endDynRendering(frame);
        frameMgr->submitAndPresent(frame, {});
    }

    ctx->getGraphicsQueue()->waitIdle();
    frameMgr.reset(); swapchain.reset(); ctx.reset();
    SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
