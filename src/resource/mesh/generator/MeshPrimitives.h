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

struct PlaneDesc {
    // Size in X (width) and Z (depth) directions (plane lies on XZ, centered at origin, normal +Y)
    glm::vec2 size {1.f,1.f};
    // Number of quads along each axis (>=1). vertices = (subdivX+1)*(subdivZ+1)
    uint32 subdivX = 1;
    uint32 subdivZ = 1;
    glm::vec4 color {1.f,1.f,1.f,1.f};
};

struct CylinderDesc {
    float radius = 0.5f;      // Radius of top and bottom
    float height = 1.f;       // Total height (y from -h/2 .. +h/2)
    uint32 radialSegments = 32;  // Around
    uint32 heightSegments = 1;   // Along height
    bool capTop = true;
    bool capBottom = true;
    glm::vec4 color {1.f,1.f,1.f,1.f};
};

struct ConeDesc {
    float radius = 0.5f;      // Base radius (at y = -h/2)
    float height = 1.f;       // Apex at y = +h/2
    uint32 radialSegments = 32;
    uint32 heightSegments = 1; // Segments along slant (excluding apex duplication)
    bool capBase = true;       // Add base disk
    glm::vec4 color {1.f,1.f,1.f,1.f};
};

struct AxesGizmoDesc {
    // Total length of each axis (shaft + cone tip)
    float axisLength = 1.0f;
    // Radius of cylindrical shaft
    float shaftRadius = 0.02f;
    // Cone dimensions for arrow head
    float coneRadius = 0.05f;
    float coneHeight = 0.15f;
    // Tessellation
    uint32 radialSegments = 16;
    uint32 shaftHeightSegments = 1; // along shaft
    // Colors (default RGB axes)
    glm::vec4 colorX {1.f,0.f,0.f,1.f};
    glm::vec4 colorY {0.f,1.f,0.f,1.f};
    glm::vec4 colorZ {0.f,0.f,1.f,1.f};
    // If true, each axis starts at origin and extends in + direction. Otherwise centered (not implemented yet).
    bool startAtOrigin = true;
};

MeshSrcData MakeCubeSrcData(const CubeDesc& d = {});
MeshSrcData MakeUVSphereSrcData(const UVSphereDesc& d = {});
MeshSrcData MakeIcoSphereSrcData(const IcoSphereDesc& d = {});
MeshSrcData MakeTeapotSrcData(const TeapotDesc& d = {});
MeshSrcData MakePlaneSrcData(const PlaneDesc& d = {});
MeshSrcData MakeCylinderSrcData(const CylinderDesc& d = {});
MeshSrcData MakeConeSrcData(const ConeDesc& d = {});
MeshSrcData MakeAxesGizmoSrcData(const AxesGizmoDesc& d = {});

// Add more (torus, grid, etc.)

} // namespace resource::mesh::primitives