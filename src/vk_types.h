// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

// TODO: move this to a non-vulkan specific header
#define int32  int32_t
#define int16  int16_t
#define int8   int8_t
#define uint32 uint32_t
#define uint16 uint16_t
#define uint8  uint8_t

struct AllocatedImage {
    VkImage image;
    VkImageView view;
    VmaAllocation allocation;
    VkFormat format;
    VkExtent3D extent;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;
	ComputePushConstants data;
};

struct alignas(16) Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draw
struct MeshDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct GPUSceneData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDir;
	glm::vec4 sunlightColor;
};

enum class AlphaMode : uint8 {
    Opaque,
    Masked,
    Transparent,
    Other
};

struct MaterialPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

struct MaterialInstance {
	MaterialPipeline* pipeline;
	VkDescriptorSet descriptorSet;
	AlphaMode alphaMode;
};

struct Bounds {
    glm::vec3 origin;
    glm::vec3 extents;
    float sphereRadius;
};

struct RenderObject {
	uint32 indexCount;
	uint32 firstIndex;
	VkBuffer indexBuffer;

	// Pipeline and descriptor sets
	MaterialInstance* material;

    Bounds bounds;
	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
    std::vector<RenderObject> OpaqueDrawData;
    std::vector<RenderObject> TransparentDrawData;
};

class IRenderable {
	virtual void gather_draw_data(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propogate
// to them
struct Node : public IRenderable {
    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    std::string name;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto& child : children) {
            child->refreshTransform(worldTransform);
        }
    }

    virtual void gather_draw_data(const glm::mat4& topMatrix, DrawContext& ctx) override
    {
        for (auto& child : children) {
            child->gather_draw_data(topMatrix, ctx);
        }
    }
};

