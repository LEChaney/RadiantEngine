#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColorFactors;
layout(location = 2) in vec2 inUV;

layout (location = 0) out vec4 outColor;

void main() {
    vec3 normal = normalize(inNormal); // Normalize after interpolation

    float lightValue = max(dot(normal, sceneData.sunlightDir.xyz), 0.1f);

    vec3 albedo = inColorFactors * texture(colorTex, inUV).rgb;
    vec3 ambient = albedo * sceneData.ambientColor.rgb;
    outColor = vec4(albedo * lightValue * sceneData.sunlightColor.w + ambient, 1.0f);
}