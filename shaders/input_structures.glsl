layout (set = 0, binding = 0) uniform ScaneData {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 ambientColor;
    vec4 sunlightDir;
    vec4 sunlightColor;
} sceneData;

layout (set = 1, binding = 0) uniform GLTFMaterialData {
    vec4 colorFactors;
    vec4 metalRoughFactors;
} materialData;

layout (set = 1, binding = 1) uniform sampler2D colorTex;
layout (set = 1, binding = 2) uniform sampler2D metalRoughTex;
