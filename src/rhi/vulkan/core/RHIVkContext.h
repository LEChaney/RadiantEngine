#pragma once
#include "rhi/interface/core/RHIContext.h"
#include "core/CoreDefs.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"
#include <functional>
#include <vk_mem_alloc.h>

namespace RHI {
class RHIDescriptorBufferCreateInfo;
struct RHIDescriptorWriterOps;
}

namespace RHI::Vulkan {

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

class RHIVkContext : public RHIContext {
public:
    enum class ValidationLevel {
        Info,
        Warning,
        Error
    };
    using ValidationCallback = std::function<void(const char* message, ValidationLevel level)>;

    // Validation mode to control how validation layers are configured
    enum class ValidationMode {
        None,          // No validation layers
        Standard,      // Standard CPU validation via VK_LAYER_KHRONOS_validation
        GpuAssisted,   // GPU-assisted validation via VK_EXT_validation_features
        Auto           // Use a sensible default based on build configuration
    };
    
    static UniquePtr<RHIVkContext> createUnique(ValidationMode mode = ValidationMode::Auto);
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
    UniquePtr<RHISwapchain> createSwapchain(const RHISwapchainCreateInfo& info) override;
    UniquePtr<RHIBuffer> createBuffer(uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) override;
    UniquePtr<RHIImage> createImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage, RHIMemoryPropertyFlags memProps) override;
    UniquePtr<RHIShaderModule> createShaderModule(const Path& spvFilePath) override;
    UniquePtr<RHIShaderModule> createShaderModule(const Array<uint32>& shaderCode) override;
    UniquePtr<RHIDescriptorSetLayout> createDescriptorSetLayout(InitializerList<RHIDescriptorSetBindingDesc> bindings) override;
    UniquePtr<RHIPipelineLayout> createPipelineLayout(const RHIPipelineLayoutCreateInfo& info) override;
    UniquePtr<RHIPipeline> createComputePipeline(const RHIComputePipelineDescriptor& desc) override;
    UniquePtr<RHIPipeline> createGraphicsPipeline(const RHIGraphicsPipelineDescriptor& desc) override;
    UniquePtr<RHIDescriptorBuffer> createDescriptorBuffer(const RHIDescriptorBufferCreateInfo& createInfo) override;
    void deviceWaitIdle() override;

    // Backend ops table providers for value types
    const RHIDescriptorWriterOps* getDescriptorWriterOps() const override;

    // Vulkan RHI factory methods
    UniquePtr<RHIVkCommandBuffer> createRhiVkCommandBuffer();
    UniquePtr<RHIVkFence> createRhiVkFence();
    UniquePtr<RHIVkSemaphore> createRhiVkSemaphore();
    UniquePtr<RHIVkSwapchain> createRhiVkSwapchain(const RHISwapchainCreateInfo &info);
    UniquePtr<RHIVkBuffer> createRhiVkBuffer(uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps);
    // TODO: Finish implementation of createRhiVkImage
    UniquePtr<RHIVkImage> createRhiVkImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage, RHIMemoryPropertyFlags memProps);
    UniquePtr<RHIVkShaderModule> createRhiVkShaderModule(const Path& spvFilePath);
    UniquePtr<RHIVkShaderModule> createRhiVkShaderModule(const Array<uint32>& shaderCode);
    UniquePtr<RHIVkPipeline> createRhiVkComputePipeline(const RHIComputePipelineDescriptor& desc);
    UniquePtr<RHIVkPipeline> createRhiVkGraphicsPipeline(const RHIGraphicsPipelineDescriptor& desc);
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
    RHIVkContext(ValidationMode mode = ValidationMode::Auto);
    
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    void setupDebugMessenger();
    void queryDescriptorBufferProperties();
    void createVmaAllocator();

    // Refactored logical device helpers
    uint32 findGraphicsQueueFamily() const;
    // Populates required extensions (ray tracing treated as required). Returns true if ray query is available (optional).
    bool gatherDeviceExtensions(Array<const char*>& deviceExtensions) const;
    // (Feature enabling now handled inline in createLogicalDevice to keep interfaces simple.)
    
    ValidationMode m_validationMode = ValidationMode::None;
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
