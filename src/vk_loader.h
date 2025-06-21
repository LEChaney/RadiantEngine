#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"

#include <unordered_map>
#include <filesystem>

class VulkanEngine;

struct GeoSurface {
    uint32 startIndex;
    uint32 count;
    Bounds bounds;
    std::shared_ptr<MaterialInstance> material; // TODO: Who owns this? Where are GeoSurfaces stored?
};

struct MeshAsset {
    std::string name;
    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

class VulkanEngine;

struct LoadedGLTF : public IRenderable {

    // storage for all the data on a given glTF file
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<MaterialInstance>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    VulkanEngine* creator;

    ~LoadedGLTF() { clearAll(); };

    virtual void gather_draw_data(const glm::mat4& topMatrix, DrawContext& ctx) override;

    void delete_node(const std::string& name);
    void delete_all_nodes_except(const std::string& name);

private:

    void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(VulkanEngine* engine,std::string_view filePath);
