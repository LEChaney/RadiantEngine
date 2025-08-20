#pragma once
#include "resource/mesh/MeshSrcData.h"
#include "core/CoreDefs.h"
#include "glm/glm.hpp"

namespace resource::mesh::meshlet {

// Basic meshlet header similar to meshoptimizer output
struct Meshlet {
    uint32 vertexOffset;   // offset into global vertex remap array (or original indices if no remap)
    uint8  vertexCount;    // number of unique vertices in this meshlet
    uint32 indexOffset;    // offset into primitive (triangle index triplets) stream (indices local to meshlet)
    uint8  primitiveCount; // number of triangles
};

struct BuildParams {
    // Steam Deck (RDNA2 APU) prefers keeping a meshlet's vertex count near a single wave (64)
    // so each vertex can be processed by one thread with minimal idle lanes.
    // 64 verts also reduces shared memory/register pressure vs 128.
    uint32 maxVerticesPerMeshlet = 64;

    // Keep primitive budget high enough to amortize culling/dispatch cost.
    // 64v typically maps to ~1.8–2.2 triangles per vert in typical game meshes,
    // so 64 * 2 = 128; cap at 126 (common safe limit used by meshoptimizer/NV samples).
    uint32 maxPrimsPerMeshlet    = 126;

    // Leave spatial binning disabled by default; tune per scene (e.g. 8 for large, loose worlds).
    uint32 gridResolution = 0;
};

struct MeshletBuildResult {
    Array<Meshlet> meshlets;
    Array<uint32> vertexIndices; // concatenated indices into the unique vertices array for each meshlet (remap table)
    Array<uint8> indices;        // 3 * primitiveCount per meshlet; indices are local (0..vertexCount-1)
};

// Entry point: splits a raw indexed triangle list into meshlets using either a naive ordered cut
// or a simple spatial binning (bucket triangles by centroid, then fill meshlets per bucket).
MeshletBuildResult buildMeshlets(const MeshSrcData& src, const BuildParams& params = {});

// Packed representation for direct GPU consumption with a single vertex buffer (duplicates allowed).
// For each meshlet we copy the unique vertices it references into a contiguous subrange of 'vertices'.
// The triangle index stream stores uint8 local indices (0..vertexCount-1) into that per-meshlet subrange.
// Meshlet::vertexOffset / vertexCount define the vertex slice; Meshlet::indexOffset / primitiveCount define
// the triangle slice inside 'indices'. Allowing duplicates at meshlet boundaries avoids an indirection layer.
struct PackedMeshlets {
    Array<Vertex> vertices;  // concatenated per-meshlet unique vertices (duplicates across meshlets allowed)
    Array<uint8> indices;    // concatenated local triangle indices (3 * primitiveCount per meshlet)
    Array<Meshlet> meshlets; // meshlets with offsets pointing into above arrays
};

// Packs meshlets into contiguous vertex + local index arrays (duplicates across meshlets OK).
// No cross-meshlet dedup (simpler GPU access, possibly higher memory). No overdraw/cache reordering yet.
PackedMeshlets packMeshlets(const MeshSrcData& src, const MeshletBuildResult& build);

} // namespace resource::mesh::meshlet
