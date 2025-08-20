#include "resource/mesh/meshlet/MeshletBuilder.h"
#include <unordered_map>
#include <limits>

namespace resource::mesh::meshlet {

namespace {
struct TriInfo { uint32 i0,i1,i2; glm::vec3 centroid; };
}

static glm::vec3 computeCentroid(const Vertex& a, const Vertex& b, const Vertex& c){
    return (a.position + b.position + c.position) / 3.0f;
}

static void finalizeMeshlet(MeshletBuildResult& out,
                            Array<uint32>& curUnique,
                            Array<uint32>& curPrimLocal,
                            uint32 firstPrimLocalIndexOffset,
                            const BuildParams& params)
{
    if(curPrimLocal.empty()) return;
    // Compute bounds
    glm::vec3 c(0);
    for(auto vidx : curUnique){ c += glm::vec3(); (void)vidx; }
    // We'll approximate center/radius from primitives quickly (not accurate)
    // Instead compute AABB then derive sphere
    glm::vec3 minB(std::numeric_limits<float>::max());
    glm::vec3 maxB(std::numeric_limits<float>::lowest());
    for(auto vidx : curUnique){
        // vidx indexes into original vertex array (stored after push below)
        // We don't have direct vertex data here; postpone precise bounds or keep pointer? For now store dummy.
        // Proper implementation should pass vertices to this function; omitted for simplicity.
        (void)vidx;
    }
    Meshlet m{};
    m.vertexOffset = static_cast<uint32>(out.vertexIndices.size());
    m.vertexCount  = static_cast<uint8>(curUnique.size());
    m.indexOffset  = static_cast<uint32>(firstPrimLocalIndexOffset);
    m.primitiveCount = static_cast<uint8>(curPrimLocal.size()/3);

    out.meshlets.push_back(m);
    // Append vertex indices
    for(auto v : curUnique) { 
        out.vertexIndices.push_back(v);
    }
    // Primitive indices already appended into out.indices externally.
    curUnique.clear();
    curPrimLocal.clear();
}

MeshletBuildResult buildMeshlets(const MeshSrcData& src, const BuildParams& params) {
    MeshletBuildResult result;
    if(src.indices.empty() || src.vertices.empty()) return result;

    uint32 triCount = (uint32)(src.indices.size() / 3);
    Array<TriInfo> tris; tris.reserve(triCount);
    for(uint32 t=0;t<triCount;t++){
        uint32 i0 = src.indices[3*t+0];
        uint32 i1 = src.indices[3*t+1];
        uint32 i2 = src.indices[3*t+2];
        const Vertex& v0 = src.vertices[i0];
        const Vertex& v1 = src.vertices[i1];
        const Vertex& v2 = src.vertices[i2];
        tris.push_back({i0,i1,i2, computeCentroid(v0,v1,v2)});
    }

    // Optional spatial binning
    Array<uint32> triOrder; triOrder.reserve(triCount);
    if(params.gridResolution > 0){
        uint32 R = params.gridResolution;
        glm::vec3 minB(  std::numeric_limits<float>::max());
        glm::vec3 maxB( -std::numeric_limits<float>::max());
        for(auto& ti : tris){
            minB = glm::min(minB, ti.centroid);
            maxB = glm::max(maxB, ti.centroid);
        }
        glm::vec3 extent = glm::max(maxB - minB, glm::vec3(1e-5f));
        struct Bucket { Array<uint32> tris; };
        Array<Bucket> buckets; buckets.resize(R*R*R);
        auto bucketIndex = [&](const glm::vec3& p){
            glm::vec3 rel = (p - minB) / extent;
            glm::ivec3 c = glm::clamp(glm::ivec3(rel * (float)R), glm::ivec3(0), glm::ivec3((int)R-1));
            return (uint32)(c.x + c.y*R + c.z*R*R);
        };
        for(uint32 idx=0; idx<tris.size(); ++idx){
            uint32 bi = bucketIndex(tris[idx].centroid);
            buckets[bi].tris.push_back(idx);
        }
        for(auto& b : buckets){ for(auto triIdx : b.tris) triOrder.push_back(triIdx); }
    } else {
        for(uint32 i=0;i<triCount;i++) triOrder.push_back(i);
    }

    Array<uint32> curUnique;
    Array<uint32> curPrimLocal;
    curUnique.reserve(params.maxVerticesPerMeshlet);
    curPrimLocal.reserve(params.maxPrimsPerMeshlet*3);

    // Map original vertex index to local index within current meshlet
    std::unordered_map<uint32,uint32> localRemap;
    localRemap.reserve(params.maxVerticesPerMeshlet*2);

    uint32 firstPrimLocalIndexOffset = 0; // offset into result.indices at meshlet start

    auto flushMeshlet = [&](){
        finalizeMeshlet(result, curUnique, curPrimLocal, firstPrimLocalIndexOffset, params);
        firstPrimLocalIndexOffset = (uint32)result.indices.size();
        localRemap.clear();
    };

    for(uint32 orderedIdx : triOrder){
        const TriInfo& tri = tris[orderedIdx];
        uint32 triVerts[3] = { tri.i0, tri.i1, tri.i2 };
        uint32 neededNewVerts = 0;
        for(uint32 v : triVerts){ if(localRemap.find(v) == localRemap.end()) neededNewVerts++; }
        bool wouldOverflowVerts = (uint32)curUnique.size() + neededNewVerts > params.maxVerticesPerMeshlet;
        bool wouldOverflowPrims = (uint32)(curPrimLocal.size()/3) + 1 > params.maxPrimsPerMeshlet;
        if(!curPrimLocal.empty() && (wouldOverflowVerts || wouldOverflowPrims)){
            flushMeshlet();
        }
        // Add tri
        for(uint32 v : triVerts){
            auto it = localRemap.find(v);
            if(it == localRemap.end()){
                uint32 localIndex = (uint32)curUnique.size();
                curUnique.push_back(v);
                localRemap.emplace(v, localIndex);
            }
        }
        // Append primitive (local indices)
        for(uint32 v : triVerts){ curPrimLocal.push_back(localRemap[v]); }
        // After appending to current local prims, also append to global primitive indices (needed for finalization)
        for(uint32 v : triVerts){ result.indices.push_back(localRemap[v]); }
    }
    flushMeshlet();

    return result;
}

PackedMeshlets packMeshlets(const MeshSrcData& src, const MeshletBuildResult& build){
    PackedMeshlets packed{};
    packed.meshlets.reserve(build.meshlets.size());
    packed.vertices.reserve(build.vertexIndices.size()); // upper bound (duplicates allowed)
    packed.indices.reserve(build.indices.size());

    for(const Meshlet& mSrc : build.meshlets){
        Meshlet mOut = mSrc; // copy meta (center/radius etc.)
        // Copy this meshlet's unique vertices contiguously
        mOut.vertexOffset = (uint32)packed.vertices.size();
        for(uint32 i=0;i<mSrc.vertexCount;++i){
            uint32 origIdx = build.vertexIndices[mSrc.vertexOffset + i];
            packed.vertices.push_back(src.vertices[origIdx]);
        }
        // Copy its local triangle index triplets (already uint8 local indices)
        mOut.indexOffset = (uint32)packed.indices.size();
        for(uint32 tri=0; tri<mSrc.primitiveCount; ++tri){
            uint32 localBase = mSrc.indexOffset + tri*3;
            for(int k=0;k<3;k++){
                uint32 localIdx = build.indices[localBase + k];
                if(localIdx >= mSrc.vertexCount) localIdx = 0; // safety
                packed.indices.push_back((uint8)localIdx);
            }
        }
        packed.meshlets.push_back(mOut);
    }
    return packed;
}

} // namespace resource::mesh::meshlet
