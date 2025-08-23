// multimesh_task.ts.slang
// Task shader: emits up to THREAD_COUNT (meshlet,instance) pairs per amplification group.
// CPU precomputes a GroupRecord table (1 entry per amplification group) so we avoid per-group search.
// @entry TaskMain as_6_5 spirv

#ifndef THREAD_COUNT
#define THREAD_COUNT 64
#endif
// Amplification group size (threads = AS_GROUP_SIZE)
#ifndef MAX_PAIRS_PER_TASK
#define MAX_PAIRS_PER_TASK THREAD_COUNT // each mesh dispatch launched from one thread in group after compaction
#endif

struct MeshletGPU { uint32_t vertexOffset; uint8_t vertexCount; uint32_t indexOffset; uint8_t primitiveCount; };
struct InstanceData { float4x4 model; float4 color; };
struct GroupRecord { 
    uint32_t meshletStart;
    uint32_t meshletCount;
    uint32_t instanceStart;
    uint32_t instanceCount;
    uint32_t pairStart;
    uint32_t pairCount;
};
struct MeshletBounds { float4 centerRadius; };
struct Pair { uint32_t meshletIndex; uint32_t instanceIndex; };
struct TaskPayload { uint32_t pairCount; Pair pairs[MAX_PAIRS_PER_TASK]; };

struct PushData { float4x4 ViewProj; };
[[vk::push_constant]] PushData PC;

[[vk::binding(0,0)]] StructuredBuffer<MeshletGPU>    gMeshlets       : register(t0, space0);
[[vk::binding(3,0)]] StructuredBuffer<InstanceData>  gInstances      : register(t3, space0);
[[vk::binding(4,0)]] StructuredBuffer<GroupRecord>   gGroups         : register(t4, space0);
[[vk::binding(5,0)]] StructuredBuffer<MeshletBounds> gBounds         : register(t5, space0); // (optional / future culling)

groupshared TaskPayload gPayload;

[shader("amplification")]
[numthreads(THREAD_COUNT,1,1)]
void TaskMain(uint gtid : SV_GroupThreadID, uint3 groupID : SV_GroupID)
{
    uint gid = groupID.x; // amplification group index
    GroupRecord GR = gGroups[gid];
    uint emitCount = GR.pairCount; // 0..THREAD_COUNT

    // Early out if no pairs to emit
    if(emitCount == 0){
        if(gtid==0) {
            DispatchMesh(0,1,1,gPayload);
        }
        return;
    }

    if(gtid < emitCount){
        uint pairOffset = GR.pairStart + gtid; // global pair index within draw's cartesian product
        uint meshletRel = pairOffset / GR.instanceCount;
        uint instRel    = pairOffset % GR.instanceCount;
        uint meshletIndex  = GR.meshletStart + meshletRel;
        uint instanceIndex = GR.instanceStart + instRel;
        gPayload.pairs[gtid] = Pair(meshletIndex, instanceIndex);
    }
    GroupMemoryBarrierWithGroupSync();
    if(gtid==0){
        gPayload.pairCount = emitCount;
        DispatchMesh(emitCount,1,1,gPayload);
    }
}
