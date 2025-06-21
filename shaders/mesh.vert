#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColorFactors;
layout (location = 2) out vec2 outUV;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout (buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

// push constants
layout (push_constant) uniform PushConstants {
    mat4 model;
    VertexBuffer vertexBuffer;
} pushConstants;

void main()
{
    Vertex v = pushConstants.vertexBuffer.vertices[gl_VertexIndex];

    vec4 position = vec4(v.position, 1.0f);
    gl_Position = sceneData.viewProj * pushConstants.model * position;

    // TODO: handle non-uniform scaling
    outNormal = (pushConstants.model * vec4(v.normal, 0.0f)).xyz;
    outColorFactors = v.color.rgb * materialData.colorFactors.rgb;
    outUV = vec2(v.uv_x, v.uv_y);
}