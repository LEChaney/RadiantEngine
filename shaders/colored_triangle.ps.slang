// Fragment (pixel) shader converted from GLSL `colored_triangle.frag`
// Original GLSL:
//   layout(location = 0) in  vec3 inColor;
//   layout(location = 0) out vec4 outFragColor;
//   outFragColor = vec4(inColor, 1.0);
// Mesh/vertex stage should output COLOR0 (or vk::location(0)) matching this.

// If you share structs across stages, you could move this to a common .slang file.
// @entry ColoredTrianglePS ps_6_0 spirv

[shader("fragment")] // or "pixel"; "fragment" is portable across Vulkan/DX targets
float4 ColoredTrianglePS(float3 color : COLOR0) : SV_Target0
{
    return float4(color, 1.0);
}
