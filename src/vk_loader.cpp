#include "vk_loader.h"

#include "vk_engine.h"
#include "vk_initializers.h"
#include "vk_types.h"

#include "stb_image.h"

#include "glm/gtx/quaternion.hpp"

#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/parser.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/util.hpp"

#include <iostream>

std::optional<AllocatedImage> load_image(VulkanEngine* engine, fastgltf::Asset& asset, fastgltf::Image& image, const std::filesystem::path& base_dir)
{
    AllocatedImage newImage {};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor {
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading
                                                    // local files.

                // Resolve the URI relative to the glTF file's directory
                std::filesystem::path resolved_path = base_dir / std::filesystem::path(filePath.uri.path().begin(), filePath.uri.path().end());
                const std::string path = resolved_path.string();
                unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,true);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                    &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,true);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                                               // specify LoadExternalBuffers, meaning all buffers
                                               // are already loaded into a vector.
                               [](auto& arg) {},
                               [&](fastgltf::sources::Vector& vector) {
                                   unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset,
                                       static_cast<int>(bufferView.byteLength),
                                       &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;

                                       newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_SAMPLED_BIT,true);

                                       stbi_image_free(data);
                                   }
                               } },
                    buffer.data);
            },
        },
        image.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
        return {};
    } else {
        return newImage;
    }
}

VkFilter extract_filter(fastgltf::Filter filter)
{
    switch (filter) {
    // nearest samplers
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

    // linear samplers
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
    switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(VulkanEngine *engine, std::string_view filePath)
{
    fmt::print("Loading GLTF: {}", filePath);

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    scene->creator = engine;
    LoadedGLTF& file = *scene.get();

    fastgltf::Parser parser {};

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;
    // fastgltf::Options::LoadExternalImages;

    fastgltf::GltfDataBuffer data;
    if (!data.loadFromFile(filePath)) {
        std::cerr << "Failed to load glTF file: " << filePath << std::endl;
        return {};
    }

    fastgltf::Asset gltf;

    std::filesystem::path path = filePath;
    std::filesystem::path base_dir = path.parent_path();

    auto type = fastgltf::determineGltfFileType(&data);
    if (type == fastgltf::GltfType::glTF) {
        auto load = parser.loadGLTF(&data, path.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    } else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadBinaryGLTF(&data, path.parent_path(), gltfOptions);
        if (load) {
            gltf = std::move(load.get());
        } else {
            std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
            return {};
        }
    } else {
        std::cerr << "Failed to determine glTF container" << std::endl;
        return {};
    }

    // we can estimate the descriptors we will need accurately
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = { 
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 } 
    };

    file.descriptorPool.init(engine->_device, gltf.materials.size(), sizes);

    // load samplers
    for (fastgltf::Sampler& sampler : gltf.samplers) {

        VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
        sampl.maxLod = VK_LOD_CLAMP_NONE;
        sampl.minLod = 0;

        sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(engine->_device, &sampl, nullptr, &newSampler);

        file.samplers.push_back(newSampler);
    }

    // load all textures
    for (fastgltf::Image& image : gltf.images) {
		std::optional<AllocatedImage> img = load_image(engine, gltf, image, base_dir);

		if (img.has_value()) {
			file.images.push_back(*img);
		}
		else {
			// we failed to load, so lets give the slot a default white texture to not
			// completely break loading
			file.images.push_back(engine->_errorCheckerboardImage);
			std::cout << "gltf failed to load texture " << image.name << std::endl;
		}
    }

    // create buffer to hold the material data
    file.materialDataBuffer = engine->create_buffer(
        sizeof(GLTFMetallicRoughness::MaterialConstants) * gltf.materials.size(),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    int data_index = 0;
    GLTFMetallicRoughness::MaterialConstants* mappedMaterialConstants = 
        (GLTFMetallicRoughness::MaterialConstants*)file.materialDataBuffer.info.pMappedData;

    // load materials
    for (fastgltf::Material& mat : gltf.materials) {
        std::shared_ptr<MaterialInstance> newMat = std::make_shared<MaterialInstance>();
        file.materials.push_back(newMat);

        AlphaMode alphaMode = AlphaMode::Opaque;
        if (mat.alphaMode == fastgltf::AlphaMode::Blend) {
            alphaMode = AlphaMode::Transparent;
        } else if (mat.alphaMode == fastgltf::AlphaMode::Mask) {
            alphaMode = AlphaMode::Masked;
        }

        GLTFMetallicRoughness::MaterialConstants constants;
        constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
        constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
        constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
        constants.colorFactors.w = mat.pbrData.baseColorFactor[3];

        constants.metalRoughFactors.x = mat.pbrData.metallicFactor;
        constants.metalRoughFactors.y = mat.pbrData.roughnessFactor;
        // write material parameters to mapped buffer
        mappedMaterialConstants[data_index] = constants;

        GLTFMetallicRoughness::MaterialResources materialResources;
        // default the material textures
        materialResources.colorImage = engine->_whiteImage;
        materialResources.colorSampler = engine->_defaultSamplerLinear;
        materialResources.metalRoughImage = engine->_whiteImage;
        materialResources.metalRoughSampler = engine->_defaultSamplerLinear;

        // set the uniform buffer for the material data
        materialResources.dataBuffer = file.materialDataBuffer.buffer;
        materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallicRoughness::MaterialConstants);

        // grab textures from gltf file
        if (mat.pbrData.baseColorTexture.has_value()) {
            auto& baseColorTextureInfo = mat.pbrData.baseColorTexture.value();
            const auto& texture = gltf.textures[baseColorTextureInfo.textureIndex];
            assert(texture.imageIndex.has_value() && "Texture missing imageIndex");
            size_t img = texture.imageIndex.value();
            size_t sampler = texture.samplerIndex.has_value() ? texture.samplerIndex.value() : SIZE_MAX;

            materialResources.colorImage = file.images[img];
            materialResources.colorSampler = (sampler != SIZE_MAX) ? file.samplers[sampler] : engine->_defaultSamplerLinear;
        }
        if (mat.pbrData.metallicRoughnessTexture.has_value()) {
            auto& metallicRoughnessTextureInfo = mat.pbrData.metallicRoughnessTexture.value();
            const auto& texture = gltf.textures[metallicRoughnessTextureInfo.textureIndex];
            assert(texture.imageIndex.has_value() && "Texture missing imageIndex");
            size_t img = texture.imageIndex.value();
            size_t sampler = texture.samplerIndex.has_value() ? texture.samplerIndex.value() : SIZE_MAX;

            materialResources.metalRoughImage = file.images[img];
            materialResources.metalRoughSampler = (sampler != SIZE_MAX) ? file.samplers[sampler] : engine->_defaultSamplerLinear;
        }
        // build material
        *newMat = engine->_metalRoughMaterial.write_material(engine->_device, alphaMode, materialResources, file.descriptorPool);

        data_index++;
    }

    // use the same vectors for all meshes so that the memory doesnt reallocate as
    // often
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    // Load meshes
    for (fastgltf::Mesh& mesh : gltf.meshes) {
        std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
        file.meshes.push_back(newmesh);
        newmesh->name = mesh.name;

        // clear the mesh arrays each mesh, we dont want to merge them by error
        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count;

            size_t initial_vtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexaccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor,
                    [&](std::uint32_t idx) {
                        indices.push_back(idx + initial_vtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newvtx;
                        newvtx.position = v;
                        newvtx.normal = { 1, 0, 0 };
                        newvtx.color = glm::vec4 { 1.f };
                        newvtx.uv_x = 0;
                        newvtx.uv_y = 0;
                        vertices[initial_vtx + index] = newvtx;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initial_vtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).second],
                    [&](glm::vec2 v, size_t index) {
                        vertices[initial_vtx + index].uv_x = v.x;
                        vertices[initial_vtx + index].uv_y = v.y;
                    });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {

                fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).second],
                    [&](glm::vec4 v, size_t index) {
                        vertices[initial_vtx + index].color = v;
                    });
            }

            if (p.materialIndex.has_value()) {
                newSurface.material = file.materials[p.materialIndex.value()];
            } else {
                newSurface.material = file.materials[0];
            }

            // loop the vertices of this surface, find min/max bounds
            glm::vec3 minpos = vertices[initial_vtx].position;
            glm::vec3 maxpos = vertices[initial_vtx].position;
            for (int32 i = initial_vtx; i < vertices.size(); ++i) {
                minpos = glm::min(minpos, vertices[i].position);
                maxpos = glm::max(maxpos, vertices[i].position);
            }
            // calculate origin and extents from the min/max
            newSurface.bounds.origin = (maxpos + minpos) / 2.f;
            newSurface.bounds.extents = (maxpos - minpos) / 2.f;
            newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);

            newmesh->surfaces.push_back(newSurface);
        }

        newmesh->meshBuffers = engine->upload_mesh(indices, vertices);
    }

    // load all nodes and their meshes
    for (fastgltf::Node& node : gltf.nodes) {
        std::shared_ptr<Node> newNode;

        // find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            static_cast<MeshNode*>(newNode.get())->mesh = file.meshes[*node.meshIndex];
        } else {
            newNode = std::make_shared<Node>();
        }
        newNode->name = node.name;
        file.nodes.push_back(newNode);

        std::visit(fastgltf::visitor ( 
            [&](fastgltf::Node::TransformMatrix matrix) {
                memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
            },
            [&](fastgltf::Node::TRS transform) {
                glm::vec3 tl(transform.translation[0], transform.translation[1],
                    transform.translation[2]);
                glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                    transform.rotation[2]);
                glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                glm::mat4 rm = glm::toMat4(rot);
                glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                newNode->localTransform = tm * rm * sm;
            }),
            node.transform
        );
    }

    // run loop again to setup transform hierarchy
    for (int i = 0; i < gltf.nodes.size(); i++) {
        fastgltf::Node& node = gltf.nodes[i];
        std::shared_ptr<Node>& sceneNode = file.nodes[i];

        for (auto& c : node.children) {
            sceneNode->children.push_back(file.nodes[c]);
            file.nodes[c]->parent = sceneNode;
        }
    }

    // find the top nodes, with no parents
    for (auto& node : file.nodes) {
        if (node->parent.lock() == nullptr) {
            file.topNodes.push_back(node);
            node->refreshTransform(glm::mat4 { 1.f });
        }
    }
    return scene;
}

void LoadedGLTF::gather_draw_data(const glm::mat4& topMatrix, DrawContext& ctx)
{
    // create renderables from the scenenodes
    for (auto& n : topNodes) {
        n->gather_draw_data(topMatrix, ctx);
    }
}

void LoadedGLTF::delete_node(const std::string &name)
{
    // Find the node pointer by name in the nodes vector
    auto it = std::find_if(nodes.begin(), nodes.end(),
        [&](const std::shared_ptr<Node>& n) {
            return n && n->name == name;
        });
    if (it != nodes.end()) {
        std::shared_ptr<Node> node_ptr = *it;

        // Remove from parent's children, if parent exists
        if (auto parent_ptr = node_ptr->parent.lock()) {
            auto &siblings = parent_ptr->children;
            siblings.erase(std::remove_if(siblings.begin(), siblings.end(),
                [&](const std::shared_ptr<Node>& n) {
                    return n == node_ptr;
                }), siblings.end());

            // Reattach grandchildren to parent
            for (auto &child : node_ptr->children) {
                child->parent = parent_ptr;
                siblings.push_back(child);
            }
        } else {
            // Node is a top node, so reattach its children as new top nodes
            for (auto &child : node_ptr->children) {
                child->parent.reset();
                topNodes.push_back(child);
            }
        }

        // Remove from topNodes by pointer address
        topNodes.erase(std::remove_if(topNodes.begin(), topNodes.end(),
            [&](const std::shared_ptr<Node>& n) {
                return n == node_ptr;
            }), topNodes.end());

        nodes.erase(it);
    }
}

void LoadedGLTF::delete_all_nodes_except(const std::string &name)
{
    // Find the node pointer to keep
    std::shared_ptr<Node> keep_ptr = nullptr;
    for (const auto& node_ptr : nodes) {
        if (node_ptr && node_ptr->name == name) {
            keep_ptr = node_ptr;
            break;
        }
    }

    // Collect names to delete
    std::vector<std::string> to_delete;
    for (const auto& node_ptr : nodes) {
        if (node_ptr && node_ptr->name != name) {
            to_delete.push_back(node_ptr->name);
        }
    }
    // Delete all nodes except the one with the given name
    for (const auto& n : to_delete) {
        delete_node(n);
    }
}

void LoadedGLTF::clearAll()
{
    VkDevice dv = creator->_device;

    descriptorPool.destroy_pools(dv);
    creator->destroy_buffer(materialDataBuffer);

    for (auto& meshPtr : meshes) {
        if (meshPtr) {
            creator->destroy_buffer(meshPtr->meshBuffers.indexBuffer);
            creator->destroy_buffer(meshPtr->meshBuffers.vertexBuffer);
        }
    }

    for (auto& img : images) {
        // Only destroy if it's not the error checkerboard image
        if (img.image == creator->_errorCheckerboardImage.image) {
            //dont destroy the default images
            continue;
        }
        creator->destroy_image(img);
    }

	for (auto& sampler : samplers) {
		vkDestroySampler(dv, sampler, nullptr);
    }
}
