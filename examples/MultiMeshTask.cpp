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
// TaskBatch encodes one tiling "tile" of the 2D (meshlet × instance) pair grid produced by the CPU tiler.
// Two tile forms are generated:
//  1. Meshlet batch tile: meshletCount > 1 (or ==1) AND instanceStart == 0 AND instanceCount == full per‑mesh instance count.
//     Represents consecutive meshlets, each including ALL their instances. Maximizes reuse; only produced when
//     (instancesPerMesh <= kTaskGroupSize) so multiple meshlets fit inside one workgroup.
//  2. Instance stripe tile: meshletCount == 1 and instanceCount is a slice (stripe) of that meshlet's instance range
//     starting at instanceStart. Used when a full instance span (or multiple meshlets with all instances) would exceed
//     the thread group size (instancesPerMesh > kTaskGroupSize) or for the tail stripe.
// Invariants the task shader relies on:
//  - meshletCount * instanceCount <= kTaskGroupSize (fits in one amplification group / thread group)
//  - If meshletCount > 1 then instanceStart == 0 and instanceCount == instancesPerMesh (pure batch)
//  - If meshletCount == 1 stripe may have instanceStart > 0 and 1 <= instanceCount <= instancesPerMesh
// Decoding in task shader: emitCount = meshletCount * instanceCount; thread linear index maps to
//   meshletIndex = meshletStart + (tid / instanceCount)
//   instanceIndex = instanceStart + (tid % instanceCount)
// This structure keeps per-dispatch enumeration O(1) and near‑full utilization except for edge (tail) tiles.
struct TaskBatch { uint32 meshletStart, meshletCount, instanceStart, instanceCount; };
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

int main(int argc, char** argv) {
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

    constexpr uint32 instCountPerMesh = 25;
    constexpr uint32 kTaskGroupSize = 64; // THREAD_COUNT in shader
    Array<TaskBatch> taskBatches;
    taskBatches.reserve(meshes.size() * instCountPerMesh / kTaskGroupSize); // 1 per task group
    Array<InstanceData> allInstances;
    allInstances.reserve(10000);

    uint32 vertBase=0, idxBase=0, meshletBase=0, instBase=0;
    uint32 totalTaskGroups = 0; // number of amplification groups (one dispatch group per TaskBatch)
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dist(-1.f,1.f);

    for (uint32 meshIdx = 0; meshIdx < meshes.size(); ++meshIdx) {
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

        auto meshMeshletCount = cast<uint32>(meshlets.size());
        if (instCountPerMesh > 0 && meshMeshletCount > 0) {
            uint32 taskGroupsForMesh = 0;

            // Choose tiling factors (A meshlets × B instances) maximizing A*B <= kTaskGroupSize.
            // Constraints:
            //  - 1 <= A <= meshMeshletCount
            //  - 1 <= B <= instCountPerMesh
            //  - A * B <= kTaskGroupSize
            // Special fast path: if instCountPerMesh >= kTaskGroupSize we can only emit stripes:
            //    A = 1, B = kTaskGroupSize (each tile is a slice of one meshlet's instances).
            uint32 A = 1;
            uint32 B = 1;

            if (instCountPerMesh >= kTaskGroupSize) {
                A = 1;
                B = kTaskGroupSize;
            } else {
                // Search B descending (prefer larger instance spans for vertex reuse of a meshlet)
                uint32 bestPairs = 0;
                uint32 maxB = std::min(instCountPerMesh, kTaskGroupSize);
                for (uint32 b = maxB; b >= 1; --b) {
                    uint32 a = kTaskGroupSize / b;
                    if (a == 0) {
                        continue;
                    }
                    if (a > meshMeshletCount) {
                        a = meshMeshletCount; // can't exceed remaining meshlets in first batch
                    }
                    uint32 pairs = a * b;
                    if (pairs > bestPairs) {
                        bestPairs = pairs;
                        A = a;
                        B = b;
                        if (pairs == kTaskGroupSize) {
                            break; // perfect fill
                        }
                    }
                    if (b == 1) {
                        break; // prevent unsigned wrap
                    }
                }
            }

            // Emit tiles over meshlet-major, instance-minor grid:
            // For meshlets in chunks of A, for instances in chunks of B.
            // Edge tiles (last row/column) shrink A and/or B to remaining counts.
            for (uint32 m0 = 0; m0 < meshMeshletCount; m0 += A) {
                uint32 thisA = std::min(A, meshMeshletCount - m0);
                for (uint32 i0 = 0; i0 < instCountPerMesh; i0 += B) {
                    uint32 thisB = std::min(B, instCountPerMesh - i0);
                    // Safety: ensure tile does not exceed group size
                    ASSERT(thisA * thisB <= kTaskGroupSize);
                    taskBatches.push_back({
                        .meshletStart  = meshletBase + m0,
                        .meshletCount  = thisA,
                        .instanceStart = instBase + i0,
                        .instanceCount = thisB
                    });
                    ++taskGroupsForMesh;
                }
            }

            totalTaskGroups += taskGroupsForMesh;
            std::printf("Mesh %u: meshlets=%u instances=%u tile(meshlets=%u,instances=%u) groups=%u totalPairs=%u\n",
                meshIdx, meshMeshletCount, instCountPerMesh, A, B, taskGroupsForMesh,
                meshMeshletCount * instCountPerMesh);
        }

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
    auto tileBuf     = makeStaticBuf(taskBatches.data(), taskBatches.size()*sizeof(TaskBatch), 0);
    auto instanceBuf = makeDynamicMappedBuf(allInstances.data(), allInstances.size()*sizeof(InstanceData), 0);

    // --- Pipeline & descriptors --- (task + mesh + frag)
    PipelineRes pipe;
    pipe.setLayout = ctx->createDescriptorSetLayout({
        {0, RHIDescriptorType::StorageBuffer, RHIShaderStage::Task | RHIShaderStage::Mesh},
        {1, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh},
        {2, RHIDescriptorType::StorageBuffer, RHIShaderStage::Mesh},
        {3, RHIDescriptorType::StorageBuffer, RHIShaderStage::Task | RHIShaderStage::Mesh},
        {4, RHIDescriptorType::StorageBuffer, RHIShaderStage::Task}, // TaskBatch buffer
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
       .writeStorageBuffer(4,0, tileBuf->createSlice())
       .writeStorageBuffer(5,0, boundsBuf->createSlice())
       .flush();

    glm::mat4 proj = makePerspectiveReverseZ(glm::radians(60.f), 1280.f/720.f, 0.01f, 200.f);
    proj[1][1] *= -1.f; // GLM fix for Vulkan

    bool running = true; uint64 startTicks = SDL_GetTicks64();
    while(running){
        SDL_Event ev; while(SDL_PollEvent(&ev)){ if(ev.type==SDL_QUIT) running=false; }
        float t = float(SDL_GetTicks64() - startTicks) * 0.001f;

        // Animate a subset of instances (e.g., first of each draw) by rotating
        for(size_t ii = 0; ii < allInstances.size(); ii += instCountPerMesh){
            // TODO: Templated version of buffer slice
            RHIBufferSlice gpuInstDataSlice = instanceBuf->createSlice(
                ii * sizeof(InstanceData), sizeof(InstanceData));
            auto gpuInstData = rcast<InstanceData*>(gpuInstDataSlice.getMapped());
            float ang = t * 0.5f + cast<float>(ii);
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
