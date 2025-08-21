// mesh_meshlet.ms.slang
// Meshlet-oriented mesh shader (Steam Deck tuned)
//
// Goals:
//  * Process one meshlet per workgroup (SV_GroupID = meshlet index)
//  * Use 64 threads (2 wave32s) for good occupancy on RDNA2/Steam Deck
//  * Support up to kMaxMeshletVertices / kMaxMeshletTriangles per meshlet
//  * Vertices are laid out per-meshlet contiguously (duplicates allowed across meshlets)
//  * Output triangles using local (0..vertexCount-1) indices into the meshlet's vertex range
//  * Apply MVP transform via push constant matrix
//  * Optional debug coloring based on meshlet id (toggle DEBUG_MESHLET_COLOR)
//
// Expected descriptor set 0 bindings:
//  binding 0: StructuredBuffer<MeshletGPU>   gMeshlets
//  binding 1: StructuredBuffer<Vertex>       gVertices  (concatenated per-meshlet unique vertices)
//  binding 2: StructuredBuffer<uint8_t>      gPrimitiveIndices (concatenated, 3 * primitiveCount per meshlet, local vertex indices)
//
// Push constants: MVP matrix (float4x4) (64 bytes)
//
// NOTE: CPU side must create a pipeline layout + descriptor set layout matching the above.
//       Existing generic mesh shader (mesh.ms.slang) remains unchanged for packed single-dispatch path.

// @entry MeshletMain ms_6_5 spirv

#ifndef DEBUG_MESHLET_COLOR
#define DEBUG_MESHLET_COLOR 1
#endif

// Workgroup sizing: keep a single definition to avoid magic numbers.
// If you tune thread count later (e.g. 32 or 128) change here only.
#ifndef MESHLET_THREAD_COUNT
#define MESHLET_THREAD_COUNT 64
#endif
// Limits (match BuildParams defaults: up to 64 verts, 126 tris)
// Nvidia recommended values: https://developer.nvidia.com/blog/advanced-api-performance-mesh-shaders/
static const uint kMaxMeshletVertices  = 64;
static const uint kMaxMeshletTriangles = 126;

struct Vertex {
    float3 position;
    float3 normal;
    float4 color;
    float2 uv;
};

struct MeshletGPU {
    uint32_t vertexOffset;   // offset into gVertices (per-meshlet vertex block)
    uint8_t  vertexCount;    // number of unique vertices in this meshlet
    uint32_t indexOffset;    // offset into gPrimitiveIndices (triplets of local uint8 indices)
    uint8_t  primitiveCount; // number of triangles
};

struct CameraPC {
    float4x4 MVP;
};
[[vk::push_constant]] CameraPC Cam;

[[vk::binding(0,0)]] StructuredBuffer<MeshletGPU> gMeshlets         : register(t0, space0);
[[vk::binding(1,0)]] StructuredBuffer<Vertex>     gVertices         : register(t1, space0);
[[vk::binding(2,0)]] StructuredBuffer<uint8_t>    gPrimitiveIndices : register(t2, space0);

struct VSOut {
    float4 position : SV_Position;
    float3 normal   : NORMAL0;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(MESHLET_THREAD_COUNT,1,1)] // MESHLET_THREAD_COUNT threads (64 = 2 wave32 groups on RDNA2 / Steam Deck)
void MeshletMain(
    uint gtid : SV_GroupThreadID,
    uint gid  : SV_GroupID,
    out vertices VSOut outVerts[kMaxMeshletVertices],
    out indices  uint3 outTris[kMaxMeshletTriangles])
{
    MeshletGPU m = gMeshlets[gid];

    SetMeshOutputCounts(m.vertexCount, m.primitiveCount);

    // Emit vertex attributes (stride across group threads if vertexCount > MESHLET_THREAD_COUNT)
    for(uint v = gtid; v < m.vertexCount; v += MESHLET_THREAD_COUNT)
    {
        Vertex src = gVertices[m.vertexOffset + v];
        VSOut dst;
        dst.position = mul(Cam.MVP, float4(src.position, 1.0));
        dst.normal   = src.normal;
        dst.uv       = src.uv;
#if DEBUG_MESHLET_COLOR
        float3 dbg = float3(
            float(gid & 1),
            float((gid >> 1) & 3) / 3.0,
            float((gid >> 3) & 7) / 7.0);
        dst.color = float4(dbg, 1.0);
#else
        dst.color = src.color;
#endif
        outVerts[v] = dst;
    }

    // Emit triangle indices (allow more triangles than threads via striding)
    for(uint t = gtid; t < m.primitiveCount; t += MESHLET_THREAD_COUNT)
    {
        uint triBase = m.indexOffset + t * 3;
        uint v0 = gPrimitiveIndices[triBase + 0];
        uint v1 = gPrimitiveIndices[triBase + 1];
        uint v2 = gPrimitiveIndices[triBase + 2];
        // (Optionally validate: v0/v1/v2 < m.vertexCount)
        outTris[t] = uint3(v0, v1, v2);
    }
}
