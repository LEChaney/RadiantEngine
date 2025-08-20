#pragma once
#include "resource/mesh/MeshSrcData.h"
#include "core/CoreDefs.h"
#include "glm/glm.hpp"

namespace resource::mesh::primitives {

struct CubeDesc {
    glm::vec3 size {1.f,1.f,1.f};
    glm::vec4 color {1.f,1.f,1.f,1.f};
};

struct UVSphereDesc {
    float radius = 0.5f;
    uint32 segments = 32;   // longitude
    uint32 rings = 16;      // latitude
    glm::vec4 color {1.f,1.f,1.f,1.f};
};

struct IcoSphereDesc {
    float radius = 0.5f;
    uint32 subdivisions = 2;
    glm::vec4 color {1.f,1.f,1.f,1.f};
};

MeshSrcData MakeCubeSrcData(const CubeDesc& d = {});
MeshSrcData MakeUVSphereSrcData(const UVSphereDesc& d = {});
MeshSrcData MakeIcoSphereSrcData(const IcoSphereDesc& d = {});

// Add more (plane, cylinder, cone, torus, axes gizmo, etc.)

} // namespace resource::mesh::primitives