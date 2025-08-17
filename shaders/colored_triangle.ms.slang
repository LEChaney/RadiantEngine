// Slang mesh shader equivalent of colored_triangle.mesh (GLSL)
// Generates a single triangle from vertex & index buffers, with push-constant color.
// @entry MeshMain ms_6_5 spirv

[[vk::binding(0,0)]] // Vulkan/Spir-V specific bindings `vk::binding(<binding#>, <set#>)`
StructuredBuffer<float4> positions : register(t0, space0);
[[vk::binding(1,0)]] // Vulkan/Spir-V specific bindings `vk::binding(<binding#>, <set#>)`
StructuredBuffer<uint>   indices   : register(t1, space0);

struct PushConstants
{
    float4 color; // rgba
};
// Apply attribute to the instance only
[[vk::push_constant]] PushConstants pc;

struct VertexOut
{
    float4 position : SV_Position;
    float3 color    : COLOR0;
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(1,1,1)]
void MeshMain(

    out vertices VertexOut outVerts[3], // max 3 vertices
    out indices uint3 outTriangles[1])  // max 1 triangle
{
    // Expect exactly 3 indices for one triangle.
    uint i0 = indices[0];
    uint i1 = indices[1];
    uint i2 = indices[2];

    SetMeshOutputCounts(3, 1);

    outVerts[0].position = float4(positions[i0].xyz, 1.0);
    outVerts[1].position = float4(positions[i1].xyz, 1.0);
    outVerts[2].position = float4(positions[i2].xyz, 1.0);

    float3 c = pc.color.rgb;
    outVerts[0].color = c;
    outVerts[1].color = c;
    outVerts[2].color = c;

    outTriangles[0] = uint3(0, 1, 2);
}