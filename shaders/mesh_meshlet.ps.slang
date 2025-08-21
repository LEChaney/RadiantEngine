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
    // Basic lighting: ambient + directional diffuse + simple specular
    float3 N = normalize(input.normal);
    // Directional light coming from above-right-front
    const float3 L = normalize(float3(0.4, 0.8, 0.6));
    float ndotl = max(dot(N, L), 0.0);
    // Simple view direction approximation (camera at +Z looking -Z in world)
    const float3 V = normalize(float3(0.0, 0.0, 1.0));
    float3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    float ambient = 0.15;
    float diffuseScale = 0.85;
    float3 base = input.color.rgb;
    float3 lit = base * (ambient + diffuseScale * ndotl) + spec * 0.25;
    return float4(lit, input.color.a);
}