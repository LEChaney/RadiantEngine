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

} // namespace resource::mesh::primitives