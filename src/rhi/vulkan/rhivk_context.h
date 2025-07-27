#pragma once
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
    
    static UniquePtr<RHIVKContext> create_unique(bool enableValidation = false);
    ~RHIVKContext() override;

    RHIQueue* get_graphics_queue() override;
    RHIVKQueue* get_vk_graphics_queue();

    // Factory methods for creating RHI objects
    UniquePtr<RHICommandBuffer> create_command_buffer() override;
    UniquePtr<RHIFence> create_fence() override;
    UniquePtr<RHISemaphore> create_semaphore() override;
    UniquePtr<RHISwapchain> create_swapchain(SDL_Window* window, uint32 width, uint32 height, uint32 buffer_count) override;
    UniquePtr<RHIBuffer> create_buffer(uint64 size, RHIBufferUsage usage, RHIMemoryProperty mem_props) override;
    UniquePtr<RHIImage> create_image(uint32 width, uint32 height, RHIFormat format, RHIImageUsage usage, RHIMemoryProperty mem_props) override;

    // Vulkan RHI factory methods
    UniquePtr<RHIVKCommandBuffer> create_vk_command_buffer();
    UniquePtr<RHIVKFence> create_vk_fence();
    UniquePtr<RHIVKSemaphore> create_vk_semaphore();
    UniquePtr<RHIVKSwapchain> create_vk_swapchain(SDL_Window* window, uint32 width, uint32 height, uint32 image_count);
    UniquePtr<RHIVKBuffer> create_vk_buffer(uint64 size, RHIBufferUsage usage, RHIMemoryProperty mem_props);
    // TODO: Finish implementation of create_vk_image
    UniquePtr<RHIVKImage> create_vk_image(uint32 width, uint32 height, RHIFormat format, RHIImageUsage usage, RHIMemoryProperty mem_props);

    // Vulkan object accessors
    const VkInstance& get_vk_instance() const { return m_instance; }
    const VkPhysicalDevice& get_vk_physical_device() const { return m_physical_device; }
    const VkDevice& get_vk_device() const { return m_device; }
    const VkCommandPool& get_vk_command_pool() const { return m_command_pool; }

    // Set a custom validation callback (thread-unsafe, for test/dev only)
    static void set_validation_callback(ValidationCallback cb);

protected:
    RHIVKContext(bool enableValidation = false);
    RHIVKContext(const RHIVKContext&) = delete;
    RHIVKContext& operator=(const RHIVKContext&) = delete;
    RHIVKContext(RHIVKContext&&) = delete;
    RHIVKContext& operator=(RHIVKContext&&) = delete;

private:
    void create_instance();
    void pick_physical_device();
    void create_logical_device();
    void create_command_pool();
    void setup_debug_messenger();
    bool validation_enabled_ = false;

    VkInstance m_instance{};
    VkPhysicalDevice m_physical_device{};
    VkDevice m_device{};
    VkCommandPool m_command_pool{};
    VkDebugUtilsMessengerEXT m_debug_messenger{};

    UniquePtr<RHIVKQueue> m_rhi_graphics_queue;
};

} // namespace rhi::vulkan
