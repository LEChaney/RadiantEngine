// multimesh_task.ms.slang
// Mesh shader consuming payload produced by task shader
// @entry MeshletMain ms_6_5 spirv

#ifndef THREAD_COUNT
#define THREAD_COUNT 64
#endif
static const uint32_t kMaxMeshletVertices  = 64;
static const uint32_t kMaxMeshletTriangles = 126;

struct Vertex { float3 position; float3 normal; float4 color; float2 uv; };
struct MeshletGPU { uint32_t vertexOffset; uint8_t vertexCount; uint32_t indexOffset; uint8_t primitiveCount; };
struct InstanceData { float4x4 model; float4 color; };
struct Pair { uint32_t meshletIndex; uint32_t instanceIndex; };
struct TaskPayload { uint32_t pairCount; Pair pairs[256]; }; // must match MAX_PAIRS_PER_TASK
struct PushData { float4x4 ViewProj; };
[[vk::push_constant]] PushData PC;

[[vk::binding(0,0)]] StructuredBuffer<MeshletGPU>   gMeshlets         : register(t0, space0);
[[vk::binding(1,0)]] StructuredBuffer<Vertex>       gVertices         : register(t1, space0);
[[vk::binding(2,0)]] StructuredBuffer<uint8_t>      gPrimitiveIndices : register(t2, space0);
[[vk::binding(3,0)]] StructuredBuffer<InstanceData> gInstances        : register(t3, space0);

struct VSOut {
    float4 position : SV_Position;
    float3 normal   : NORMAL0;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(THREAD_COUNT,1,1)]
void MeshletMain(uint gtid : SV_GroupThreadID,
                 uint gid  : SV_GroupID,
                  in payload  TaskPayload payload,
                 out vertices VSOut outVerts[kMaxMeshletVertices],
                 out indices  uint3 outTris[kMaxMeshletTriangles])
{
    Pair pair = payload.pairs[gid];
    MeshletGPU m = gMeshlets[pair.meshletIndex];
    InstanceData inst = gInstances[pair.instanceIndex];

    SetMeshOutputCounts(m.vertexCount, m.primitiveCount);

    for(uint v = gtid; v < m.vertexCount; v += THREAD_COUNT){
        Vertex src = gVertices[m.vertexOffset + v];
        VSOut dst;
        float4 wPos = mul(inst.model, float4(src.position,1));
    dst.position = mul(PC.ViewProj, wPos);
        dst.normal = mul((float3x3)inst.model, src.normal);
        dst.uv = src.uv;
        dst.color = src.color * inst.color;
        outVerts[v] = dst;
    }

    for(uint t = gtid; t < m.primitiveCount; t += THREAD_COUNT){
        uint triBase = m.indexOffset + t*3;
        outTris[t] = uint3(
            gPrimitiveIndices[triBase+0],
            gPrimitiveIndices[triBase+1],
            gPrimitiveIndices[triBase+2]);
    }
}
