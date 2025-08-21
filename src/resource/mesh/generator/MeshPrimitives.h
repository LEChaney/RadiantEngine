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

struct TeapotDesc {
    // Uniform scale applied to canonical Utah teapot control points (original units roughly ~3.15 height)
    float scale = 0.5f; // brings teapot to about similar scale as spheres (radius ~0.5)
    // Number of segments per parametric direction for each bicubic patch (>=1). 10 gives decent quality.
    uint32 tessellation = 10;
    glm::vec4 color {1.f,1.f,1.f,1.f};
};

MeshSrcData MakeCubeSrcData(const CubeDesc& d = {});
MeshSrcData MakeUVSphereSrcData(const UVSphereDesc& d = {});
MeshSrcData MakeIcoSphereSrcData(const IcoSphereDesc& d = {});
MeshSrcData MakeTeapotSrcData(const TeapotDesc& d = {});

// Add more (plane, cylinder, cone, torus, axes gizmo, etc.)

} // namespace resource::mesh::primitives