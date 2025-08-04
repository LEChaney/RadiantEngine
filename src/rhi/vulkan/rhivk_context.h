#pragma once
#include <vk_mem_alloc.h>
#include "rhi/rhi_context.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>
#include <functional>

namespace rhi::vulkan {

class RHIVKQueue;
class RHIVKCommandBuffer;
class RHIVKFence;
class RHIVKSemaphore;
class RHIVKSwapchain;
class RHIVKBuffer;
class RHIVKImage;

class RHIVKContext : public RHIContext {
public:
    enum class ValidationLevel {
        Info,
        Warning,
        Error
    };
    using ValidationCallback = std::function<void(const char* message, ValidationLevel level)>;
    
    static UniquePtr<RHIVKContext> createUnique(bool enableValidation = false);
    ~RHIVKContext() override;

    RHIVKContext(const RHIVKContext&) = delete;
    RHIVKContext& operator=(const RHIVKContext&) = delete;
    RHIVKContext(RHIVKContext&&) = delete;
    RHIVKContext& operator=(RHIVKContext&&) = delete;

    RHIQueue* getGraphicsQueue() override;
    RHIVKQueue* getVkGraphicsQueue();

    // Factory methods for creating RHI objects
    UniquePtr<RHICommandBuffer> createCommandBuffer() override;
    UniquePtr<RHIFence> createFence() override;
    UniquePtr<RHISemaphore> createSemaphore() override;
    UniquePtr<RHISwapchain> createSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 imageCount) override;
    UniquePtr<RHIBuffer> createBuffer(uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) override;
    UniquePtr<RHIImage> createImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage, RHIMemoryPropertyFlags memProps) override;

    // Vulkan RHI factory methods
    UniquePtr<RHIVKCommandBuffer> createRhiVkCommandBuffer();
    UniquePtr<RHIVKFence> createRhiVkFence();
    UniquePtr<RHIVKSemaphore> createRhiVkSemaphore();
    UniquePtr<RHIVKSwapchain> createRhiVkSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 imageCount);
    UniquePtr<RHIVKBuffer> createRhiVkBuffer(uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps);
    // TODO: Finish implementation of createRhiVkImage
    UniquePtr<RHIVKImage> createRhiVkImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage, RHIMemoryPropertyFlags memProps);

    // Vulkan object accessors
    const VkInstance& getVkInstance() const { return m_instance; }
    const VkPhysicalDevice& getVkPhysicalDevice() const { return m_physicalDevice; }
    const VkDevice& getVkDevice() const { return m_device; }
    const VkCommandPool& getVkCommandPool() const { return m_commandPool; }

    // VMA allocator accessors
    VmaAllocator getVmaAllocator() const { return m_vmaAllocator; }

    // Set a custom validation callback (thread-unsafe, for test/dev only)
    static void setValidationCallback(ValidationCallback callback);

private:
    RHIVKContext(bool enableValidation = false);
    
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    void setupDebugMessenger();
    
    bool m_validationEnabled = false;
    VkInstance m_instance{};
    VkPhysicalDevice m_physicalDevice{};
    VkDevice m_device{};
    VkCommandPool m_commandPool{};
    VkDebugUtilsMessengerEXT m_debugMessenger{};
    VmaAllocator m_vmaAllocator = VK_NULL_HANDLE;

    UniquePtr<RHIVKQueue> m_rhiGraphicsQueue;
};

} // namespace rhi::vulkan
