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

class RHIVKContext : public RHIContext {
public:
    enum class ValidationLevel {
        Info,
        Warning,
        Error
    };
    using ValidationCallback = std::function<void(const char* message, ValidationLevel level)>;

    RHIVKContext(bool enableValidation = false);
    ~RHIVKContext() override;

    RHIQueue* get_graphics_queue() override;

    // Factory methods for creating RHI objects
    // TODO: Move creation logic to constructors of RHI objects where possible
    UniquePtr<RHICommandBuffer> create_command_buffer() override;
    UniquePtr<RHIFence> create_fence() override;
    UniquePtr<RHISemaphore> create_semaphore() override;
    UniquePtr<RHISwapchain> create_swapchain(SDL_Window* window, uint32_t width, uint32_t height, uint32_t buffer_count) override;
    UniquePtr<RHIBuffer> create_buffer(uint64_t size, BufferUsage usage, MemoryProperty mem_props) override;

    // Vulkan factory methods
    UniquePtr<RHIVKCommandBuffer> create_vk_command_buffer();
    UniquePtr<RHIVKFence> create_vk_fence();
    UniquePtr<RHIVKSemaphore> create_vk_semaphore();
    UniquePtr<RHIVKSwapchain> create_vk_swapchain(SDL_Window* window, uint32_t width, uint32_t height, uint32_t image_count);
    UniquePtr<RHIVKBuffer> create_vk_buffer(uint64_t size, BufferUsage usage, MemoryProperty mem_props);

    // Vulkan object accessors
    const VkInstance& get_vk_instance() const { return m_instance; }
    const VkPhysicalDevice& get_vk_physical_device() const { return m_physical_device; }
    const VkDevice& get_vk_device() const { return m_device; }
    const VkQueue& get_vk_graphics_queue() const { return m_graphics_queue; }
    const uint32_t& get_vk_graphics_queue_family() const { return m_graphics_queue_family; }
    const VkCommandPool& get_vk_command_pool() const { return m_command_pool; }

    // Set a custom validation callback (thread-unsafe, for test/dev only)
    static void set_validation_callback(ValidationCallback cb);

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
    VkQueue m_graphics_queue{};
    uint32_t m_graphics_queue_family = 0;
    VkCommandPool m_command_pool{};
    VkDebugUtilsMessengerEXT m_debug_messenger{};

    UniquePtr<RHIVKQueue> m_rhi_graphics_queue;
};

} // namespace rhi::vulkan
