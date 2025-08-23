// mesh.ps.slang
// Pass-through pixel shader: outputs interpolated vertex color
// @entry main ps_6_0 spirv

struct PSInput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL0;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    return input.color;
}