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
#include "rhi/interface/queue/RHIQueue.h"
#include "rhi/interface/command/RHICommandBuffer.h"
#include "core/CoreDefs.h"

#include <glm/gtc/matrix_transform.hpp>
#include <SDL.h>
#include <cstdio>
#include <random>

using namespace RHI;
using namespace resource::mesh::primitives;
using namespace resource::mesh::meshlet;
using Renderer::FrameManager;

// --- Data structures matching shader layout ---
struct MeshletGPU { uint32 vertexOffset; uint8 vertexCount; uint32 indexOffset; uint8 primitiveCount; };
struct InstanceData { glm::mat4 model; glm::vec4 color; };
struct DrawRecord { uint32 meshletStart, meshletCount, instanceStart, instanceCount, groupBase; };
struct GroupRecord { uint32 meshletStart; uint32 meshletCount; uint32 instanceStart; uint32 instanceCount; uint32 pairStart; uint32 pairCount; };
struct MeshletBounds { glm::vec4 centerRadius; };

struct PipelineRes {
    UniquePtr<RHIDescriptorSetLayout> setLayout;
    UniquePtr<RHIPipelineLayout> pipelineLayout;
    UniquePtr<RHIDescriptorBuffer> descriptorBuffer;
    UniquePtr<RHIShaderModule> taskShader;
    UniquePtr<RHIShaderModule> meshShader;
    UniquePtr<RHIShaderModule> fragShader;
    UniquePtr<RHIPipeline> pipeline;
};

static glm::mat4 makePerspectiveReverseZ(float fovyRadians, float aspect, float zNear, float zFar){
    float f = 1.0f / std::tan(fovyRadians * 0.5f);
    glm::mat4 m(0.0f);
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = zNear / (zFar - zNear);
    m[2][3] = -1.0f;
    m[3][2] = (zFar * zNear) / (zFar - zNear);
    return m;
}

int main(int argc, char** argv){
    (void)argc; (void)argv;
    if(SDL_Init(SDL_INIT_VIDEO) != 0){ std::printf("SDL init failed: %s\n", SDL_GetError()); return 1; }
    SDL_Window* window = SDL_CreateWindow("MultiMeshTask", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if(!window){ std::puts("Failed create window"); return 1; }

    auto validationMode = Vulkan::RHIVkContext::ValidationMode::Auto; // default validation mode
    auto ctx = Vulkan::RHIVkContext::createUnique(validationMode);

    RHISwapchainCreateInfo sci{
        .window = window, .width=1280, .height=720, .imageCount=3
    };
    auto swapchain = ctx->createSwapchain(sci);
    auto frameMgr = FrameManager::createUnique(ctx.get(), swapchain.get(), 2);

    // --- Build all primitive meshes ---
    Array<PackedMeshlets> meshes;
    // Teapot, Cube, UVSphere, IcoSphere, Plane, Cylinder, Cone, AxesGizmo
    TeapotDesc teapot{ .scale = 0.4f, .tessellation = 8 };
    CubeDesc cube{ .size = {0.5f,0.5f,0.5f} };
    UVSphereDesc uvSphere{ .radius = 0.35f, .segments = 24, .rings = 12 };
    IcoSphereDesc ico{ .radius = 0.35f, .subdivisions = 2 };
    PlaneDesc plane{ .size = {1.2f,1.2f}, .subdivX = 4, .subdivZ = 4 };
    CylinderDesc cyl{ .radius=0.25f, .height=0.7f, .radialSegments=40, .heightSegments=4 };
    ConeDesc cone{ .radius=0.3f, .height=0.6f, .radialSegments=40, .heightSegments=1 };
    AxesDesc axes{ .axisLength = 0.8f, .coneRadius=0.05f, .coneHeight=0.15f };

    auto addMesh = [&](const MeshSrcData& src){
        BuildParams bp{ .gridResolution = 0 }; // sequential
        meshes.push_back(packMeshlets(src, buildMeshlets(src, bp)));
    };

    addMesh(MakeTeapotSrcData(teapot));
    addMesh(MakeCubeSrcData(cube));
    addMesh(MakeUVSphereSrcData(uvSphere));
    addMesh(MakeIcoSphereSrcData(ico));
    addMesh(MakePlaneSrcData(plane));
    addMesh(MakeCylinderSrcData(cyl));
    addMesh(MakeConeSrcData(cone));
    addMesh(MakeAxesSrcData(axes));

    // --- Concatenate packed data ---
    Array<Vertex> allVertices; allVertices.reserve(1<<20);
    Array<uint8> allPrimIdx; allPrimIdx.reserve(1<<20);
    Array<MeshletGPU> allMeshlets; allMeshlets.reserve(1<<16);
    Array<MeshletBounds> bounds; bounds.reserve(1<<16);

    Array<DrawRecord> drawRecords; drawRecords.reserve(meshes.size());
    Array<InstanceData> allInstances; allInstances.reserve(10000);
    Array<GroupRecord> groupRecords; groupRecords.reserve(drawRecords.size()); // one per amplification group

    uint32 vertBase=0, idxBase=0, meshletBase=0, instBase=0;
    uint32 totalTaskGroups = 0; // number of amplification groups (one dispatch group per entry in groupRecords)
    constexpr uint32 kGroupSize = 64; // THREAD_COUNT in shader
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist(-1.f,1.f);

    for (size_t meshIdx = 0; meshIdx < meshes.size(); ++meshIdx) {
        auto&[vertices, indices, meshlets] = meshes[meshIdx];
        // Append vertices & indices`
        allVertices.insert(allVertices.end(), vertices.begin(), vertices.end());
        allPrimIdx.insert(allPrimIdx.end(), indices.begin(), indices.end());
        // Meshlets adjusted
        for(auto [vertexOffset, vertexCount, indexOffset, primitiveCount] : meshlets){
            MeshletGPU ml{
                vertexOffset + vertBase,
                vertexCount,
                indexOffset + idxBase,
                primitiveCount
            };
            allMeshlets.push_back(ml);

            // Compute bounds (simple centroid sphere)
            glm::vec3 c(0);
            for(uint32 v = 0; v < ml.vertexCount; ++v) {
                c += allVertices[ml.vertexOffset + v].position;
            }
            c /= cast<float>(vertexCount);
            float r = 0.f;
            for(uint32 v = 0; v < vertexCount; ++v) {
                r = std::max(r, glm::length(allVertices[ml.vertexOffset + v].position - c));
            }
            bounds.push_back({ glm::vec4(c,r) });
        }

        // Create many instances arranged in shells per mesh type
        constexpr uint32 instCountPerMesh = 350; // stress (large to test chunking)
        for (uint32 instIdx = 0; instIdx < instCountPerMesh; ++instIdx) {
            float frac = cast<float>(instIdx) / cast<float>(instCountPerMesh);
            float ang = frac * glm::two_pi<float>() * (1.f + 0.2f * cast<float>(meshIdx));
            float rad = 3.0f + 0.15f * cast<float>(meshIdx) + 0.002f * cast<float>(instIdx);
            glm::vec3 pos = { std::cos(ang)*rad, 0.2f*dist(rng), std::sin(ang)*rad };
            glm::mat4 model(1.f);
            model = glm::translate(model, pos);
            float s = 0.8f + 0.4f * std::sin(cast<float>(instIdx) * 0.1f + cast<float>(meshIdx));
            model = glm::scale(model, glm::vec3(s));
            InstanceData instData{
                model,
                glm::vec4(
                    0.5f+0.5f*dist(rng),
                    0.5f+0.5f*dist(rng),
                    0.5f+0.5f*dist(rng),
                    1.f
                )
            };
            allInstances.push_back(instData);
        }

        auto meshletCount = cast<uint32>(meshlets.size());
        // Compute number of amplification groups needed for this draw
        uint64 pairCount64 = uint64(meshletCount) * uint64(instCountPerMesh);
        uint32 groupsForDraw = pairCount64 == 0 ? 0 : uint32((pairCount64 + kGroupSize - 1) / kGroupSize);
        drawRecords.push_back({ meshletBase, meshletCount, instBase, instCountPerMesh, totalTaskGroups });
        // Emit group records
        for(uint32 g=0; g<groupsForDraw; ++g){
            uint64 pairStart = uint64(g) * kGroupSize;
            uint64 remaining = pairCount64 - pairStart;
            uint32 emit = remaining < kGroupSize ? uint32(remaining) : kGroupSize;
            groupRecords.push_back({ meshletBase, meshletCount, instBase, instCountPerMesh, uint32(pairStart), emit });
        }
        totalTaskGroups += groupsForDraw;

        // Advance bases
        vertBase = cast<uint32>(allVertices.size());
        idxBase = cast<uint32>(allPrimIdx.size());
        meshletBase = cast<uint32>(allMeshlets.size());
        instBase = cast<uint32>(allInstances.size());
    }

    // --- Buffer creation helpers ---
    auto makeStaticBuf = [&](const void* data, size_t size, RHIBufferUsageFlags usage){
        auto buf = ctx->createBuffer(
            size,
            usage
                | RHIBufferUsage::TransferDst
                | RHIBufferUsage::StorageBuffer
                | RHIBufferUsage::ShaderDeviceAddress,
            RHIMemoryProperty::DeviceLocal
        );
        // staging (simple: create temp host buffer)
        auto staging = ctx->createBuffer(
            size, 
            RHIBufferUsage::TransferSrc, 
            RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent
        );
        void* m = staging->map();
        std::memcpy(m,data,size);
        // record copy
        auto cmd = ctx->createCommandBuffer();
        cmd->begin();
        cmd->copyBuffer(staging.get(), buf.get(), 0, 0, size);
        cmd->end();
        ctx->getGraphicsQueue()->submitAndWait(cmd.get());
        return buf;
        // staging buffer destroyed
    };
    auto makeDynamicMappedBuf = [&](const void* data, size_t size, RHIBufferUsageFlags usage){
        auto buf = ctx->createBuffer(size, usage | RHIBufferUsage::StorageBuffer | RHIBufferUsage::ShaderDeviceAddress,
            RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent);
        buf->map(); // persistently mapped
        if (data) {
            std::memcpy(buf->getMapped(), data, size);
        }
        return buf;
    };

    auto meshletsBuf = makeStaticBuf(allMeshlets.data(), allMeshlets.size()*sizeof(MeshletGPU), 0);
    auto verticesBuf = makeStaticBuf(allVertices.data(), allVertices.size()*sizeof(Vertex), 0);
    auto primIdxBuf  = makeStaticBuf(allPrimIdx.data(), allPrimIdx.size()*sizeof(uint8), 0);
    auto boundsBuf   = makeStaticBuf(bounds.data(), bounds.size()*sizeof(MeshletBounds), 0);
    auto groupBuf    = makeDynamicMappedBuf(groupRecords.data(), groupRecords.size()*sizeof(GroupRecord), 0);
    auto instanceBuf = makeDynamicMappedBuf(allInstances.data(), allInstances.size()*sizeof(InstanceData), 0);

    // --- Pipeline & descriptors --- (task + mesh + frag)
    PipelineRes pipe;
    pipe.setLayout = ctx->createDescriptorSetLayout({
        {0, RHIDescriptorType::StorageBuffer, RHIShaderStage::Task | RHIShaderStage::Mesh},
        {1, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh},
        {2, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh},
        {3, RHIDescriptorType::StorageBuffer, RHIShaderStage::Task | RHIShaderStage::Mesh},
        {4, RHIDescriptorType::StorageBuffer, RHIShaderStage::Task},
        {5, RHIDescriptorType::StorageBuffer, RHIShaderStage::Task},
    });
    pipe.descriptorBuffer = ctx->createDescriptorBuffer({ .sizeBytes = 512*1024 });
    pipe.pipelineLayout = ctx->createPipelineLayout({
        .setLayouts = { pipe.setLayout.get() },
        .pushConstantRanges = { { .stages = RHIShaderStage::Task | RHIShaderStage::Mesh, .offset = 0, .size = sizeof(glm::mat4) } }
    });
    pipe.taskShader = ctx->createShaderModule("shaders/multimesh_task.ts.slang.spv");
    pipe.meshShader = ctx->createShaderModule("shaders/multimesh_task.ms.slang.spv");
    pipe.fragShader = ctx->createShaderModule("shaders/mesh_meshlet_lit.ps.slang.spv");

    RHIGraphicsPipelineDescriptor gpDesc{};
    gpDesc.layout = pipe.pipelineLayout.get();
    gpDesc.taskShader = pipe.taskShader.get();
    gpDesc.meshShader = pipe.meshShader.get();
    gpDesc.fragmentShader = pipe.fragShader.get();
    gpDesc.colorFormat = swapchain->getColorFormat();
    gpDesc.depthFormat = swapchain->getDepthFormat();
    pipe.pipeline = ctx->createGraphicsPipeline(gpDesc);

    auto set = pipe.descriptorBuffer->allocateSet(pipe.setLayout.get(), "MultiMeshTaskSet");
    set.writeStorageBuffer(0,0, meshletsBuf->createSlice())
       .writeStorageBuffer(1,0, verticesBuf->createSlice())
       .writeStorageBuffer(2,0, primIdxBuf->createSlice())
       .writeStorageBuffer(3,0, instanceBuf->createSlice())
       .writeStorageBuffer(4,0, groupBuf->createSlice())
       .writeStorageBuffer(5,0, boundsBuf->createSlice())
       .flush();

    glm::mat4 proj = makePerspectiveReverseZ(glm::radians(60.f), 1280.f/720.f, 0.01f, 200.f);
    proj[1][1] *= -1.f; // GLM fix for Vulkan

    bool running = true; uint64 startTicks = SDL_GetTicks64();
    while(running){
        SDL_Event ev; while(SDL_PollEvent(&ev)){ if(ev.type==SDL_QUIT) running=false; }
        float t = float(SDL_GetTicks64() - startTicks) * 0.001f;

        // Animate a subset of instances (e.g., first of each draw) by rotating
        for(size_t d = 0; d < drawRecords.size(); ++d){
            auto& dr = drawRecords[d];
            if(dr.instanceCount == 0) {
                continue;
            }
            // TODO: Templated version of buffer slice
            RHIBufferSlice gpuInstDataSlice = instanceBuf->createSlice(
                dr.instanceStart * sizeof(InstanceData), dr.instanceCount * sizeof(InstanceData));
            auto gpuInstData = rcast<InstanceData*>(gpuInstDataSlice.getMapped());
            float ang = t * 0.5f + cast<float>(d);
            glm::mat4 m = glm::rotate(glm::mat4(1.f), ang, glm::vec3(0,1,0));
            m = glm::scale(m, glm::vec3(1.2f));
            gpuInstData->model = m;
        }

        auto frame = frameMgr->acquireFrame();
        frameMgr->beginDynRendering(frame);

        glm::vec3 eye = { std::sin(t*0.25f)*10.f, 6.f + 1.5f*std::sin(t*0.7f), std::cos(t*0.25f)*4.f };
        glm::mat4 view = glm::lookAt(eye, glm::vec3(0,0,0), glm::vec3(0,1,0));
        glm::mat4 viewProj = proj * view;

        frame.cmd->bindGraphicsPipeline(pipe.pipeline.get());
        frame.cmd->bindDescriptorBuffers({ pipe.descriptorBuffer.get() });
        frame.cmd->bindDescriptorSets(
            { { .setIndex = 0, .set = set } },
            pipe.pipelineLayout.get(),
            RHIPipelineBindPoint::Graphics
        );
        struct PushData { glm::mat4 vp; } push{ viewProj };
        frame.cmd->pushConstants(
            pipe.pipelineLayout.get(),
            RHIShaderStage::Task | RHIShaderStage::Mesh,
            0, sizeof(PushData),
            &push
        );
        frame.cmd->dispatchMesh(totalTaskGroups, 1, 1);

        frameMgr->endDynRendering(frame);
        frameMgr->submitAndPresent(frame);
    }

    ctx->deviceWaitIdle();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
