#pragma once
#include "core/CoreDefs.h"
#include "glm/glm.hpp"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 color;
    glm::vec2 uv;
};

struct MeshSrcData {
    Array<Vertex> vertices;
    Array<uint32> indices;
};
