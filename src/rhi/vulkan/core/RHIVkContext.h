#pragma once
#include "rhi/interface/core/RHIContext.h"
#include "core/CoreDefs.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"
#include <functional>
#include <vk_mem_alloc.h>

namespace rhi {
class RHIDescriptorBufferCreateInfo;
struct RHIDescriptorWriterOps; // forward declare ops
struct RHIDescriptorSetLayoutBuilderOps; // forward declare builder ops
}

namespace rhi::vulkan {

class RHIVkQueue;
class RHIVkCommandBuffer;
class RHIVkFence;
class RHIVkSemaphore;
class RHIVkSwapchain;
class RHIVkBuffer;
class RHIVkImage;
class RHIVkShaderModule;
class RHIVkPipeline;
class RHIVkDescriptorBuffer;
class RHIVkPipelineLayoutBuilder;

class RHIVkContext : public RHIContext {
public:
    enum class ValidationLevel {
        Info,
        Warning,
        Error
    };
    using ValidationCallback = std::function<void(const char* message, ValidationLevel level)>;
    
    static UniquePtr<RHIVkContext> createUnique(bool enableValidation = false);
    ~RHIVkContext() override;

    RHIVkContext(const RHIVkContext&) = delete;
    RHIVkContext& operator=(const RHIVkContext&) = delete;
    RHIVkContext(RHIVkContext&&) = delete;
    RHIVkContext& operator=(RHIVkContext&&) = delete;

    RHIQueue* getGraphicsQueue() override;
    RHIVkQueue* getVkGraphicsQueue();

    // Factory methods for creating RHI objects
    UniquePtr<RHICommandBuffer> createCommandBuffer() override;
    UniquePtr<RHIFence> createFence() override;
    UniquePtr<RHISemaphore> createSemaphore() override;
    UniquePtr<RHISwapchain> createSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 imageCount, RHIImageUsageFlags extraImageUsage = 0) override;
    UniquePtr<RHIBuffer> createBuffer(uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) override;
    UniquePtr<RHIImage> createImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage, RHIMemoryPropertyFlags memProps) override;
    UniquePtr<RHIPipelineLayoutBuilder> createPipelineLayoutBuilder() override;
    UniquePtr<RHIShaderModule> createShaderModule(const Path& spvFilePath) override;
    UniquePtr<RHIShaderModule> createShaderModule(const Array<uint32>& shaderCode) override;
    UniquePtr<RHIPipeline> createComputePipeline(const RHIComputePipelineDescriptor& desc) override;
    UniquePtr<RHIDescriptorBuffer> createDescriptorBuffer(const RHIDescriptorBufferCreateInfo& createInfo) override;

    // Backend ops table providers for value types
    const RHIDescriptorWriterOps* getDescriptorWriterOps() const override;
    const RHIDescriptorSetLayoutBuilderOps* getDescriptorSetLayoutBuilderOps() const override;

    // Vulkan RHI factory methods
    UniquePtr<RHIVkCommandBuffer> createRhiVkCommandBuffer();
    UniquePtr<RHIVkFence> createRhiVkFence();
    UniquePtr<RHIVkSemaphore> createRhiVkSemaphore();
    UniquePtr<RHIVkSwapchain> createRhiVkSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 imageCount, RHIImageUsageFlags extraImageUsage = 0);
    UniquePtr<RHIVkBuffer> createRhiVkBuffer(uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps);
    // TODO: Finish implementation of createRhiVkImage
    UniquePtr<RHIVkImage> createRhiVkImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage, RHIMemoryPropertyFlags memProps);
    UniquePtr<RHIVkPipelineLayoutBuilder> createRhiVkPipelineLayoutBuilder();
    UniquePtr<RHIVkShaderModule> createRhiVkShaderModule(const Path& spvFilePath);
    UniquePtr<RHIVkShaderModule> createRhiVkShaderModule(const Array<uint32>& shaderCode);
    UniquePtr<RHIVkPipeline> createRhiVkComputePipeline(const RHIComputePipelineDescriptor& desc);
    UniquePtr<RHIVkDescriptorBuffer> createRhiVkDescriptorBuffer(const RHIDescriptorBufferCreateInfo& createInfo);

    // Vulkan object accessors
    const VkInstance& getVkInstance() const { return m_instance; }
    const VkPhysicalDevice& getVkPhysicalDevice() const { return m_physicalDevice; }
    const VkDevice& getVkDevice() const { return m_device; }
    const VkCommandPool& getVkCommandPool() const { return m_commandPool; }
    const VkPhysicalDeviceDescriptorBufferPropertiesEXT& getVkDescriptorBufferProperties() const { return m_descBufferProps; }

    // VMA allocator accessors
    VmaAllocator getVmaAllocator() const { return m_vmaAllocator; }

    // Set a custom validation callback (thread-unsafe, for test/dev only)
    static void setValidationCallback(ValidationCallback callback);

private:
    RHIVkContext(bool enableValidation = false);
    
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    void setupDebugMessenger();
    void queryDescriptorBufferProperties();
    void createVmaAllocator();
    
    bool m_validationEnabled = false;
    VkInstance m_instance{};
    VkPhysicalDevice m_physicalDevice{};
    VkDevice m_device{};
    VkPhysicalDeviceDescriptorBufferPropertiesEXT m_descBufferProps{};
    VkCommandPool m_commandPool{};
    VkDebugUtilsMessengerEXT m_debugMessenger{};
    VmaAllocator m_vmaAllocator = VK_NULL_HANDLE;

    UniquePtr<RHIVkQueue> m_rhiGraphicsQueue;
};

} // namespace rhi::vulkan
