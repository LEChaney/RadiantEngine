// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "camera.h"

#include <vulkan/vulkan_core.h>

#include <cstdint>

constexpr unsigned int FRAME_OVERLAP = 2;

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()> && function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); ++it) {
			(*it)();
		}
		deletors.clear();
	}
};

struct FrameData {
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;
	VkSemaphore swapchainSemaphore;
	VkSemaphore renderSemaphore;
	VkFence renderFence;

	DescriptorAllocatorGrowable frameDescriptorAllocator;

	DeletionQueue deletionQueue;
};

struct GLTFMetallicRoughness {
	MaterialPipeline opaquePipeline;
	MaterialPipeline maskedPipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout descriptorSetLayout;

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metalRoughFactors;
		// padding, we need it anyway for uniform buffers
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32 dataBufferOffset;
	};

	DescriptorWriter writer;

	void build_pipelines(VulkanEngine* engine);
	void destroy_pipelines(VkDevice device);

	MaterialInstance write_material(VkDevice device, AlphaMode pass, const MaterialResources& resources, DescriptorAllocatorGrowable descriptorAllocator);
};

struct MeshNode : public Node {
    std::shared_ptr<MeshAsset> mesh;

    virtual void gather_draw_data(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct EngineStats {
    float frametime;
    int triangle_count;
    int drawcall_count;
    float scene_update_time;
    float mesh_CPU_draw_time;
};

class VulkanEngine {
public:
	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	VkExtent2D _windowExtent{ 1700 , 900 };

	// Vulkan handles
	VkInstance _instance; // Vulkan context
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU; // Physical device handle
	VkDevice _device; // Device driver handle
	VkSurfaceKHR _surface; // Vulkan window surface

	// Vulkan swapchain handles
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;
	VkExtent2D _drawExtent;
	float _renderScale = 1.0f;
	bool _resizeRequested = false;

	// Immediate submit structures
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;
	
	VkQueue _graphicsQueue;
	uint32 _graphicsQueueFamily;

	struct SDL_Window* _window{ nullptr };

	FrameData _frames[FRAME_OVERLAP];
	FrameData& get_current_frame() { 
		return _frames[_frameNumber % FRAME_OVERLAP];
	}

	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	VkDescriptorSet _backgroundDescriptorSet;

	VmaAllocator _allocator;
	DeletionQueue _mainDeletionQueue;

	DescriptorAllocatorGrowable _globalDescriptorAllocator;
	VkDescriptorSetLayout _backgroundDescriptorSetLayout;
	VkDescriptorSetLayout _gpuSceneDataDescriptorSetLayout;
	VkDescriptorSetLayout _singleImageDescriptorSetLayout;

	VkPipelineLayout _computePipelineLayout;
	VkPipeline _meshPipeline;

	GPUSceneData _sceneData;
	std::vector<ComputeEffect> _backgroundEffects;
	int32 _currentBackgroundEffect = 0;

	std::string _selectedMap = "structure";

	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	GLTFMetallicRoughness _metalRoughMaterial;

	DrawContext _mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> _loadedScenes;

	Camera _mainCamera;

	EngineStats _stats;

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);

	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_image(void* rawData, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(const AllocatedImage& image);
	
	GPUMeshBuffers upload_mesh(std::span<uint32> indices, std::span<Vertex> vertices);

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();
	void draw_background(VkCommandBuffer cmd);
	void draw_geometry(VkCommandBuffer cmd);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void update_render_scene();

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	void resize_swapchain();

	//run main loop
	void run();

private:
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();

	void create_swapchain(uint32 width, uint32 height);
	void destroy_swapchain();

	void init_descriptors();

	void init_pipelines();
	void init_background_pipelines();
	void init_test_mesh_pipeline();

	void init_imgui();

	void init_default_data();
};
