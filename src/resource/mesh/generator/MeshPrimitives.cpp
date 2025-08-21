#include "resource/mesh/generator/MeshPrimitives.h"
#include "glm/gtc/constants.hpp"
#include <algorithm>

namespace resource::mesh::primitives {

namespace {
// Push a triangle ensuring counter-clockwise winding (front face) for shapes centered at origin.
// Uses face normal vs centroid direction heuristic (works for convex closed meshes centered at origin).
inline void pushTriCCW(MeshSrcData& m, uint32 a, uint32 b, uint32 c){
    const glm::vec3& A = m.vertices[a].position;
    const glm::vec3& B = m.vertices[b].position;
    const glm::vec3& C = m.vertices[c].position;
    glm::vec3 faceN = glm::cross(B - A, C - A); // orientation sign
    glm::vec3 centroid = (A + B + C);
    if(glm::dot(faceN, centroid) < 0.0f){
        std::swap(b,c); // flip to make it CCW relative to outward direction
    }
    m.indices.push_back(a); m.indices.push_back(b); m.indices.push_back(c);
}
} // namespace

MeshSrcData MakeCubeSrcData(const CubeDesc& d) {
    MeshSrcData out;
    const glm::vec3 h = d.size * 0.5f;

    // 8 corners
    glm::vec3 p[8] = {
        {-h.x,-h.y,-h.z},{ h.x,-h.y,-h.z},
        { h.x, h.y,-h.z},{-h.x, h.y,-h.z},
        {-h.x,-h.y, h.z},{ h.x,-h.y, h.z},
        { h.x, h.y, h.z},{-h.x, h.y, h.z}
    };

    // Each face (position, normal, uv, color=white)
    auto addFace = [&](int32 i0, int32 i1, int32 i2, int32 i3, glm::vec3 n){
        uint32 base = (uint32)out.vertices.size();
    glm::vec4 color = d.color;
        glm::vec2 uvs[4] = {{0,0},{1,0},{1,1},{0,1}};
        int32 idxs[4] = {i0,i1,i2,i3};
        for(int32 k=0;k<4;k++){
            Vertex v;
            v.position = p[idxs[k]];
            v.normal = n;
            v.color = color;
            v.uv = uvs[k];
            out.vertices.push_back(v);
        }
        // Two tris: (0,1,2) and (2,3,0) inserted CCW check
        pushTriCCW(out, base+0, base+1, base+2);
        pushTriCCW(out, base+2, base+3, base+0);
    };

    addFace(4,5,6,7, { 0, 0, 1}); // +Z
    addFace(1,0,3,2, { 0, 0,-1}); // -Z
    addFace(0,4,7,3, {-1, 0, 0}); // -X
    addFace(5,1,2,6, { 1, 0, 0}); // +X
    addFace(3,7,6,2, { 0, 1, 0}); // +Y
    addFace(0,1,5,4, { 0,-1, 0}); // -Y

    return out;
}

MeshSrcData MakeUVSphereSrcData(const UVSphereDesc& d) {
    MeshSrcData out;
    uint32 seg = glm::max<uint32>(3, d.segments);
    uint32 ring = glm::max<uint32>(2, d.rings);
    glm::vec4 color = d.color;

    for(uint32 y=0; y<=ring; ++y){
        float v = (float)y / ring;
        float theta = v * glm::pi<float>();
        float sY = glm::sin(theta);
        float cY = glm::cos(theta);
        for(uint32 x=0; x<=seg; ++x){
            float u = (float)x / seg;
            float phi = u * glm::two_pi<float>();
            float sX = glm::sin(phi);
            float cX = glm::cos(phi);
            glm::vec3 n = {cX * sY, cY, sX * sY};
            Vertex vert;
            vert.position = n * d.radius;
            vert.normal = n;
            vert.color = color;
            vert.uv = {u, 1.f - v};
            out.vertices.push_back(vert);
        }
    }

    uint32 stride = seg + 1;
    for(uint32 y=0; y<ring; ++y){
        for(uint32 x=0; x<seg; ++x){
            uint32 i0 = y * stride + x;
            uint32 i1 = i0 + 1;
            uint32 i2 = i0 + stride;
            uint32 i3 = i2 + 1;
            if(y != 0){
                pushTriCCW(out, i0, i2, i1);
            }
            if(y != ring -1){
                pushTriCCW(out, i1, i2, i3);
            }
        }
    }
    return out;
}

MeshSrcData MakeIcoSphereSrcData(const IcoSphereDesc& d) {
    MeshSrcData out;

    // Base icosahedron (unscaled)
    const float t = (1.f + glm::sqrt(5.f)) * 0.5f; // golden ratio

    Array<glm::vec3> positions = {
        {-1, t, 0},{ 1, t, 0},{-1,-t, 0},{ 1,-t, 0},
        { 0,-1, t},{ 0, 1, t},{ 0,-1,-t},{ 0, 1,-t},
        { t, 0,-1},{ t, 0, 1},{-t, 0,-1},{-t, 0, 1}
    };

    // Normalize to unit sphere first
    for (auto &p : positions) {
        p = glm::normalize(p);
    }

    Array<glm::u32vec3> faces;
    faces.reserve(20);
    uint32 baseFaces[20][3] = {
        {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
        {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
        {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
        {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
    };
    for (auto &f : baseFaces) faces.emplace_back(f[0], f[1], f[2]);

    // Midpoint cache to avoid duplicate vertices
    std::unordered_map<uint64, uint32> midpointCache;
    midpointCache.reserve(faces.size() * 3u * (d.subdivisions + 1));

    auto edgeKey = [](uint32 a, uint32 b) -> uint64 {
        uint32 lo = glm::min(a, b);
        uint32 hi = glm::max(a, b);
        return (uint64(lo) << 32) | uint64(hi);
    };

    auto midpoint = [&](uint32 a, uint32 b) -> uint32 {
        uint64 key = edgeKey(a, b);
        auto it = midpointCache.find(key);
        if (it != midpointCache.end()) return it->second;
        glm::vec3 m = glm::normalize( (positions[a] + positions[b]) * 0.5f );
        positions.push_back(m);
        uint32 idx = static_cast<uint32>(positions.size() - 1);
        midpointCache.emplace(key, idx);
        return idx;
    };

    // Subdivide
    for (uint32 s = 0; s < d.subdivisions; ++s) {
        Array<glm::u32vec3> newFaces;
        newFaces.reserve(faces.size() * 4u);
        midpointCache.clear(); // safe to clear each level (midpoints change per topology level)
        for (auto &tri : faces) {
            uint32 a = tri.x, b = tri.y, c = tri.z;
            uint32 ab = midpoint(a, b);
            uint32 bc = midpoint(b, c);
            uint32 ca = midpoint(c, a);
            newFaces.emplace_back(a,  ab, ca);
            newFaces.emplace_back(b,  bc, ab);
            newFaces.emplace_back(c,  ca, bc);
            newFaces.emplace_back(ab, bc, ca);
        }
        faces.swap(newFaces);
    }

    // Scale to requested radius and output vertices
    out.vertices.reserve(positions.size());
    for (auto &p : positions) {
        glm::vec3 pos = p * d.radius; // already unit length
        glm::vec3 n = p; // unit normal
        Vertex vtx;
        vtx.position = pos;
        vtx.normal = n;
    vtx.color = d.color;
        // Spherical UV (simple; seam artifacts possible)
        float u = (glm::atan(n.z, n.x) / glm::two_pi<float>()) + 0.5f;
        float vtex = (glm::asin(n.y) / glm::pi<float>()) + 0.5f;
        vtx.uv = {u, 1.f - vtex};
        out.vertices.push_back(vtx);
    }

    out.indices.reserve(faces.size() * 3u);
    for (auto &tri : faces) {
        pushTriCCW(out, tri.x, tri.y, tri.z);
    }

    return out;
}

MeshSrcData MakeTeapotSrcData(const TeapotDesc& d) {
    MeshSrcData out;
    // Teapot patch indices (32 patches * 16 indices) from three.js (public domain MIT source).
    static const int patches[32][16] = {
        /*rim*/
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
        { 3,16,17,18, 7,19,20,21,11,22,23,24,15,25,26,27},
        {18,28,29,30,21,31,32,33,24,34,35,36,27,37,38,39},
        {30,40,41, 0,33,42,43, 4,36,44,45, 8,39,46,47,12},
        /*body*/
        {12,13,14,15,48,49,50,51,52,53,54,55,56,57,58,59},
        {15,25,26,27,51,60,61,62,55,63,64,65,59,66,67,68},
        {27,37,38,39,62,69,70,71,65,72,73,74,68,75,76,77},
        {39,46,47,12,71,78,79,48,74,80,81,52,77,82,83,56},
        {56,57,58,59,84,85,86,87,88,89,90,91,92,93,94,95},
        {59,66,67,68,87,96,97,98,91,99,100,101,95,102,103,104},
        {68,75,76,77,98,105,106,107,101,108,109,110,104,111,112,113},
        {77,82,83,56,107,114,115,84,110,116,117,88,113,118,119,92},
        /*handle*/
        {120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135},
        {123,136,137,120,127,138,139,124,131,140,141,128,135,142,143,132},
        {132,133,134,135,144,145,146,147,148,149,150,151, 68,152,153,154},
        {135,142,143,132,147,155,156,144,151,157,158,148,154,159,160, 68},
        /*spout*/
        {161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176},
        {164,177,178,161,168,179,180,165,172,181,182,169,176,183,184,173},
        {173,174,175,176,185,186,187,188,189,190,191,192,193,194,195,196},
        {176,183,184,173,188,197,198,185,192,199,200,189,196,201,202,193},
        /*lid*/
        {203,203,203,203,204,205,206,207,208,208,208,208,209,210,211,212},
        {203,203,203,203,207,213,214,215,208,208,208,208,212,216,217,218},
        {203,203,203,203,215,219,220,221,208,208,208,208,218,222,223,224},
        {203,203,203,203,221,225,226,204,208,208,208,208,224,227,228,209},
        {209,210,211,212,229,230,231,232,233,234,235,236,237,238,239,240},
        {212,216,217,218,232,241,242,243,236,244,245,246,240,247,248,249},
        {218,222,223,224,243,250,251,252,246,253,254,255,249,256,257,258},
        {224,227,228,209,252,259,260,229,255,261,262,233,258,263,264,237},
        /*bottom*/
        {265,265,265,265,266,267,268,269,270,271,272,273, 92,119,118,113},
        {265,265,265,265,269,274,275,276,273,277,278,279,113,112,111,104},
        {265,265,265,265,276,280,281,282,279,283,284,285,104,103,102, 95},
        {265,265,265,265,282,286,287,266,285,288,289,270, 95, 94, 93, 92},
    };
    // Control point list: 290 unique points (three.js teapotVertices / 3)
    static const float cp[][3] = {
        {1.4f,0.f,2.4f},{1.4f,-0.784f,2.4f},{0.784f,-1.4f,2.4f},{0.f,-1.4f,2.4f},
        {1.3375f,0.f,2.53125f},{1.3375f,-0.749f,2.53125f},{0.749f,-1.3375f,2.53125f},{0.f,-1.3375f,2.53125f},
        {1.4375f,0.f,2.53125f},{1.4375f,-0.805f,2.53125f},{0.805f,-1.4375f,2.53125f},{0.f,-1.4375f,2.53125f},
        {1.5f,0.f,2.4f},{1.5f,-0.84f,2.4f},{0.84f,-1.5f,2.4f},{0.f,-1.5f,2.4f},
        {-0.784f,-1.4f,2.4f},{-1.4f,-0.784f,2.4f},{-1.4f,0.f,2.4f},{-0.749f,-1.3375f,2.53125f},{-1.3375f,-0.749f,2.53125f},{-1.3375f,0.f,2.53125f},{-0.805f,-1.4375f,2.53125f},{-1.4375f,-0.805f,2.53125f},{-1.4375f,0.f,2.53125f},{-0.84f,-1.5f,2.4f},{-1.5f,-0.84f,2.4f},{-1.5f,0.f,2.4f},{-1.4f,0.784f,2.4f},{-0.784f,1.4f,2.4f},{0.f,1.4f,2.4f},{-1.3375f,0.749f,2.53125f},{-0.749f,1.3375f,2.53125f},{0.f,1.3375f,2.53125f},{-1.4375f,0.805f,2.53125f},{-0.805f,1.4375f,2.53125f},{0.f,1.4375f,2.53125f},{-1.5f,0.84f,2.4f},{-0.84f,1.5f,2.4f},{0.f,1.5f,2.4f},{0.784f,1.4f,2.4f},{1.4f,0.784f,2.4f},{0.749f,1.3375f,2.53125f},{1.3375f,0.749f,2.53125f},{0.805f,1.4375f,2.53125f},{1.4375f,0.805f,2.53125f},{0.84f,1.5f,2.4f},{1.5f,0.84f,2.4f},{1.75f,0.f,1.875f},{1.75f,-0.98f,1.875f},{0.98f,-1.75f,1.875f},{0.f,-1.75f,1.875f},{2.f,0.f,1.35f},{2.f,-1.12f,1.35f},{1.12f,-2.f,1.35f},{0.f,-2.f,1.35f},{2.f,0.f,0.9f},{2.f,-1.12f,0.9f},{1.12f,-2.f,0.9f},{0.f,-2.f,0.9f},{-0.98f,-1.75f,1.875f},{-1.75f,-0.98f,1.875f},{-1.75f,0.f,1.875f},{-1.12f,-2.f,1.35f},{-2.f,-1.12f,1.35f},{-2.f,0.f,1.35f},{-1.12f,-2.f,0.9f},{-2.f,-1.12f,0.9f},{-2.f,0.f,0.9f},{-1.75f,0.98f,1.875f},{-0.98f,1.75f,1.875f},{0.f,1.75f,1.875f},{-2.f,1.12f,1.35f},{-1.12f,2.f,1.35f},{0.f,2.f,1.35f},{-2.f,1.12f,0.9f},{-1.12f,2.f,0.9f},{0.f,2.f,0.9f},{0.98f,1.75f,1.875f},{1.75f,0.98f,1.875f},{1.12f,2.f,1.35f},{2.f,1.12f,1.35f},{1.12f,2.f,0.9f},{2.f,1.12f,0.9f},{2.f,0.f,0.45f},{2.f,-1.12f,0.45f},{1.12f,-2.f,0.45f},{0.f,-2.f,0.45f},{1.5f,0.f,0.225f},{1.5f,-0.84f,0.225f},{0.84f,-1.5f,0.225f},{0.f,-1.5f,0.225f},{1.5f,0.f,0.15f},{1.5f,-0.84f,0.15f},{0.84f,-1.5f,0.15f},{0.f,-1.5f,0.15f},{-1.12f,-2.f,0.45f},{-2.f,-1.12f,0.45f},{-2.f,0.f,0.45f},{-0.84f,-1.5f,0.225f},{-1.5f,-0.84f,0.225f},{-1.5f,0.f,0.225f},{-0.84f,-1.5f,0.15f},{-1.5f,-0.84f,0.15f},{-1.5f,0.f,0.15f},{-2.f,1.12f,0.45f},{-1.12f,2.f,0.45f},{0.f,2.f,0.45f},{-1.5f,0.84f,0.225f},{-0.84f,1.5f,0.225f},{0.f,1.5f,0.225f},{-1.5f,0.84f,0.15f},{-0.84f,1.5f,0.15f},{0.f,1.5f,0.15f},{1.12f,2.f,0.45f},{2.f,1.12f,0.45f},{0.84f,1.5f,0.225f},{1.5f,0.84f,0.225f},{0.84f,1.5f,0.15f},{1.5f,0.84f,0.15f},{-1.6f,0.f,2.025f},{-1.6f,-0.3f,2.025f},{-1.5f,-0.3f,2.25f},{-1.5f,0.f,2.25f},{-2.3f,0.f,2.025f},{-2.3f,-0.3f,2.025f},{-2.5f,-0.3f,2.25f},{-2.5f,0.f,2.25f},{-2.7f,0.f,2.025f},{-2.7f,-0.3f,2.025f},{-3.f,-0.3f,2.25f},{-3.f,0.f,2.25f},{-2.7f,0.f,1.8f},{-2.7f,-0.3f,1.8f},{-3.f,-0.3f,1.8f},{-3.f,0.f,1.8f},{-1.5f,0.3f,2.25f},{-1.6f,0.3f,2.025f},{-2.5f,0.3f,2.25f},{-2.3f,0.3f,2.025f},{-3.f,0.3f,2.25f},{-2.7f,0.3f,2.025f},{-3.f,0.3f,1.8f},{-2.7f,0.3f,1.8f},{-2.7f,0.f,1.575f},{-2.7f,-0.3f,1.575f},{-3.f,-0.3f,1.35f},{-3.f,0.f,1.35f},{-2.5f,0.f,1.125f},{-2.5f,-0.3f,1.125f},{-2.65f,-0.3f,0.9375f},{-2.65f,0.f,0.9375f},{-2.f,-0.3f,0.9f},{-1.9f,-0.3f,0.6f},{-1.9f,0.f,0.6f},{-3.f,0.3f,1.35f},{-2.7f,0.3f,1.575f},{-2.65f,0.3f,0.9375f},{-2.5f,0.3f,1.125f},{-1.9f,0.3f,0.6f},{-2.f,0.3f,0.9f},{1.7f,0.f,1.425f},{1.7f,-0.66f,1.425f},{1.7f,-0.66f,0.6f},{1.7f,0.f,0.6f},{2.6f,0.f,1.425f},{2.6f,-0.66f,1.425f},{3.1f,-0.66f,0.825f},{3.1f,0.f,0.825f},{2.3f,0.f,2.1f},{2.3f,-0.25f,2.1f},{2.4f,-0.25f,2.025f},{2.4f,0.f,2.025f},{2.7f,0.f,2.4f},{2.7f,-0.25f,2.4f},{3.3f,-0.25f,2.4f},{3.3f,0.f,2.4f},{1.7f,0.66f,0.6f},{1.7f,0.66f,1.425f},{3.1f,0.66f,0.825f},{2.6f,0.66f,1.425f},{2.4f,0.25f,2.025f},{2.3f,0.25f,2.1f},{3.3f,0.25f,2.4f},{2.7f,0.25f,2.4f},{2.8f,0.f,2.475f},{2.8f,-0.25f,2.475f},{3.525f,-0.25f,2.49375f},{3.525f,0.f,2.49375f},{2.9f,0.f,2.475f},{2.9f,-0.15f,2.475f},{3.45f,-0.15f,2.5125f},{3.45f,0.f,2.5125f},{2.8f,0.f,2.4f},{2.8f,-0.15f,2.4f},{3.2f,-0.15f,2.4f},{3.2f,0.f,2.4f},{3.525f,0.25f,2.49375f},{2.8f,0.25f,2.475f},{3.45f,0.15f,2.5125f},{2.9f,0.15f,2.475f},{3.2f,0.15f,2.4f},{2.8f,0.15f,2.4f},{0.f,0.f,3.15f},{0.8f,0.f,3.15f},{0.8f,-0.45f,3.15f},{0.45f,-0.8f,3.15f},{0.f,-0.8f,3.15f},{0.f,0.f,2.85f},{0.2f,0.f,2.7f},{0.2f,-0.112f,2.7f},{0.112f,-0.2f,2.7f},{0.f,-0.2f,2.7f},{-0.45f,-0.8f,3.15f},{-0.8f,-0.45f,3.15f},{-0.8f,0.f,3.15f},{-0.112f,-0.2f,2.7f},{-0.2f,-0.112f,2.7f},{-0.2f,0.f,2.7f},{-0.8f,0.45f,3.15f},{-0.45f,0.8f,3.15f},{0.f,0.8f,3.15f},{-0.2f,0.112f,2.7f},{-0.112f,0.2f,2.7f},{0.f,0.2f,2.7f},{0.45f,0.8f,3.15f},{0.8f,0.45f,3.15f},{0.112f,0.2f,2.7f},{0.2f,0.112f,2.7f},{0.4f,0.f,2.55f},{0.4f,-0.224f,2.55f},{0.224f,-0.4f,2.55f},{0.f,-0.4f,2.55f},{1.3f,0.f,2.55f},{1.3f,-0.728f,2.55f},{0.728f,-1.3f,2.55f},{0.f,-1.3f,2.55f},{1.3f,0.f,2.4f},{1.3f,-0.728f,2.4f},{0.728f,-1.3f,2.4f},{0.f,-1.3f,2.4f},{-0.224f,-0.4f,2.55f},{-0.4f,-0.224f,2.55f},{-0.4f,0.f,2.55f},{-0.728f,-1.3f,2.55f},{-1.3f,-0.728f,2.55f},{-1.3f,0.f,2.55f},{-0.728f,-1.3f,2.4f},{-1.3f,-0.728f,2.4f},{-1.3f,0.f,2.4f},{-0.4f,0.224f,2.55f},{-0.224f,0.4f,2.55f},{0.f,0.4f,2.55f},{-1.3f,0.728f,2.55f},{-0.728f,1.3f,2.55f},{0.f,1.3f,2.55f},{-1.3f,0.728f,2.4f},{-0.728f,1.3f,2.4f},{0.f,1.3f,2.4f},{0.224f,0.4f,2.55f},{0.4f,0.224f,2.55f},{0.728f,1.3f,2.55f},{1.3f,0.728f,2.55f},{0.728f,1.3f,2.4f},{1.3f,0.728f,2.4f},{0.f,0.f,0.f},{1.425f,0.f,0.f},{1.425f,0.798f,0.f},{0.798f,1.425f,0.f},{0.f,1.425f,0.f},{1.5f,0.f,0.075f},{1.5f,0.84f,0.075f},{0.84f,1.5f,0.075f},{0.f,1.5f,0.075f},{-0.798f,1.425f,0.f},{-1.425f,0.798f,0.f},{-1.425f,0.f,0.f},{-0.84f,1.5f,0.075f},{-1.5f,0.84f,0.075f},{-1.5f,0.f,0.075f},{-1.425f,-0.798f,0.f},{-0.798f,-1.425f,0.f},{0.f,-1.425f,0.f},{-1.5f,-0.84f,0.075f},{-0.84f,-1.5f,0.075f},{0.f,-1.5f,0.075f},{0.798f,-1.425f,0.f},{1.425f,-0.798f,0.f},{0.84f,-1.5f,0.075f},{1.5f,-0.84f,0.075f}
    };
    constexpr float fullHeight = 3.15f; // dataset Z range (Blinn-scaled)
    const float halfHeight = fullHeight * 0.5f;
    // normalize so half height becomes 1, then apply user scale => final half height = d.scale
    const float normScale = d.scale / halfHeight;
    const size_t patchCount = sizeof(patches)/sizeof(patches[0]);

    auto bezier = [](float t, float p0, float p1, float p2, float p3){
        float it = 1.f - t;
        return it*it*it*p0 + 3.f*it*it*t*p1 + 3.f*it*t*t*p2 + t*t*t*p3;
    };

    auto bezierDeriv = [](float t, float p0, float p1, float p2, float p3){
        float it = 1.f - t;
        return 3.f*( -p0*it*it + p1*(it*it - 2.f*it*t) + p2*(2.f*it*t - t*t) + p3*t*t );
    };

    uint32 T = glm::max<uint32>(2u, d.tessellation); // need >=2 for meaningful surface
    glm::vec4 color = d.color;
    // Evaluate each patch independently (no vertex sharing for correctness of normals like three.js)

    // We will build patch vertices per patch to generate indices locally then append with offset.
    for(size_t p=0; p<patchCount; ++p){
        // gather control points 4x4
        glm::vec3 cpPatch[4][4];
        for(int i=0;i<4;i++){
            for(int j=0;j<4;j++){
                int idx = patches[p][i*4+j];
                const float* c = cp[idx];
                cpPatch[i][j] = {c[0], c[1], c[2]};
            }
        }
        uint32 base = (uint32)out.vertices.size();
        for(uint32 v=0; v<=T; ++v){
            float fv = (float)v / T;
            for(uint32 u=0; u<=T; ++u){
                float fu = (float)u / T;
                // Evaluate position and partial derivatives
                glm::vec3 col[4];
                glm::vec3 dcol_du[4];
                for(int i=0;i<4;i++){
                    const glm::vec3 &p0 = cpPatch[i][0];
                    const glm::vec3 &p1 = cpPatch[i][1];
                    const glm::vec3 &p2 = cpPatch[i][2];
                    const glm::vec3 &p3 = cpPatch[i][3];
                    col[i].x = bezier(fu, p0.x,p1.x,p2.x,p3.x);
                    col[i].y = bezier(fu, p0.y,p1.y,p2.y,p3.y);
                    col[i].z = bezier(fu, p0.z,p1.z,p2.z,p3.z);
                    dcol_du[i].x = bezierDeriv(fu, p0.x,p1.x,p2.x,p3.x);
                    dcol_du[i].y = bezierDeriv(fu, p0.y,p1.y,p2.y,p3.y);
                    dcol_du[i].z = bezierDeriv(fu, p0.z,p1.z,p2.z,p3.z);
                }
                glm::vec3 pos;
                glm::vec3 du;
                glm::vec3 dv;
                // Evaluate along v (fv) across col[] as control points
                pos.x = bezier(fv, col[0].x, col[1].x, col[2].x, col[3].x);
                pos.y = bezier(fv, col[0].y, col[1].y, col[2].y, col[3].y);
                pos.z = bezier(fv, col[0].z, col[1].z, col[2].z, col[3].z);
                dv.x = bezierDeriv(fv, col[0].x, col[1].x, col[2].x, col[3].x);
                dv.y = bezierDeriv(fv, col[0].y, col[1].y, col[2].y, col[3].y);
                dv.z = bezierDeriv(fv, col[0].z, col[1].z, col[2].z, col[3].z);
                // For du, treat dcol_du as control points
                du.x = bezier(fv, dcol_du[0].x, dcol_du[1].x, dcol_du[2].x, dcol_du[3].x);
                du.y = bezier(fv, dcol_du[0].y, dcol_du[1].y, dcol_du[2].y, dcol_du[3].y);
                du.z = bezier(fv, dcol_du[0].z, dcol_du[1].z, dcol_du[2].z, dcol_du[3].z);
                // Axis transform & normalization to Y-up centered: X' = x, Y' = z - halfHeight, Z' = -y
                glm::vec3 posT { pos.x, pos.z - halfHeight, -pos.y };
                glm::vec3 duT { du.x, du.z, -du.y };
                glm::vec3 dvT { dv.x, dv.z, -dv.y };
                posT *= normScale;
                duT *= normScale;
                dvT *= normScale;
                glm::vec3 n = glm::normalize(glm::cross(duT, dvT));
                // Cusp fix: if original (x,y) nearly zero, force normal up/down
                if (std::abs(pos.x) < 1e-5f && std::abs(pos.y) < 1e-5f) {
                    n = (pos.z > halfHeight) ? glm::vec3(0,1,0) : glm::vec3(0,-1,0);
                }
                Vertex vert;
                vert.position = posT;
                vert.normal = n;
                vert.color = color;
                vert.uv = {fu, 1.f - fv};
                out.vertices.push_back(vert);
            }
        }
        // indices
        uint32 stride = T + 1;
        auto pushOriented = [&](uint32 a,uint32 b,uint32 c){
            // Ensure winding aligns with geometric normal.
            const glm::vec3 &A = out.vertices[a].position;
            const glm::vec3 &B = out.vertices[b].position;
            const glm::vec3 &C = out.vertices[c].position;
            glm::vec3 faceN = glm::cross(B-A,C-A);
            glm::vec3 avgN = out.vertices[a].normal + out.vertices[b].normal + out.vertices[c].normal;
            if(glm::dot(faceN, avgN) < 0.f) std::swap(b,c);
            out.indices.push_back(a); out.indices.push_back(b); out.indices.push_back(c);
        };
        for(uint32 v=0; v<T; ++v){
            for(uint32 u=0; u<T; ++u){
                uint32 i0 = base + v*stride + u;
                uint32 i1 = i0 + 1;
                uint32 i2 = i0 + stride;
                uint32 i3 = i2 + 1;
                // Skip degenerate triangles (at cusps) by comparing positions
                auto samePos = [&](uint32 a, uint32 b){
                    const glm::vec3 &pa = out.vertices[a].position;
                    const glm::vec3 &pb = out.vertices[b].position;
                    glm::vec3 d = pa - pb;
                    return glm::dot(d,d) < 1e-12f;
                };
                if(!(samePos(i0,i1)||samePos(i1,i2)||samePos(i0,i2)))
                    pushOriented(i0,i2,i1);
                if(!(samePos(i1,i2)||samePos(i2,i3)||samePos(i1,i3)))
                    pushOriented(i1,i2,i3);
            }
        }
    }
    return out;
}

} // namespace resource::mesh::primitives