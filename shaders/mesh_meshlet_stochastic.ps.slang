// mesh_meshlet_stochastic.ps.slang
// Stochastic Transparency demonstration pixel shader (no blending required)
// Technique: screen-space ordered dithering against per-fragment alpha to approximate
// transparency while keeping pipeline blend disabled. Each pixel either writes fully
// or is discarded based on a threshold from a Bayer matrix. Over spatial area this
// produces the perceived transparency.
//
// NOTE: This keeps depth writes enabled (same pipeline settings) so interior surfaces
// are still hidden (good for solid objects with surface translucency). For true
// stochastic transparency with accumulation / temporal smoothing you'd typically
// combine with MSAA alpha-to-coverage or per-frame jitter and possibly disable depth
// writes with weighted blended OIT. This simple version is self-contained.
//
// @entry main ps_6_0 spirv

struct PSInput
{
    float4 position : SV_Position;
    float3 normal   : NORMAL0;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

// Push constants shared with mesh stage (must match layout there)
struct CameraPC {
    float4x4 MVP;
    float Fade;
};
[[vk::push_constant]] CameraPC Cam; // Only Fade field is used here

// 8x8 Bayer (values 0..63). Chosen over 4x4 to reduce pattern repetition on large triangles.
static const uint Bayer8[64] = {
     0, 32,  8, 40,  2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44,  4, 36, 14, 46,  6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
     3, 35, 11, 43,  1, 33,  9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47,  7, 39, 13, 45,  5, 37,
    63, 31, 55, 23, 61, 29, 53, 21
};

// Simple directional light (same as lit shader for consistency)
float3 applyLighting(float3 baseColor, float3 normal){
    float3 N = normalize(normal);
    const float3 L = normalize(float3(0.4, 0.8, 0.6));
    float ndotl = max(dot(N, L), 0.0);
    const float3 V = normalize(float3(0.0, 0.0, 1.0));
    float3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    float ambient = 0.15;
    float diffuseScale = 0.85;
    return baseColor * (ambient + diffuseScale * ndotl) + spec * 0.25;
}

float4 main(PSInput input) : SV_TARGET
{
    // Global fade value provided via push constant (0 = invisible, 1 = opaque-ish thresholded)
    float baseAlpha = input.color.a; // incoming vertex alpha (normally 1)
    // Map fade (0..1) into coverage range [min,max]
    float targetAlpha = saturate(Cam.Fade) * baseAlpha;

    // Fetch Bayer threshold in [0,1)
    uint2 pix = uint2(input.position.xy) & 7; // modulo 8
    uint bIndex = pix.y * 8 + pix.x;
    float threshold = (Bayer8[bIndex] + 0.5) / 64.0; // center of bin

    // Discard if target alpha below threshold -> stochastic coverage pattern
    if(targetAlpha <= threshold)
        discard;

    float3 lit = applyLighting(input.color.rgb, input.normal);
    return float4(lit, targetAlpha);
}
