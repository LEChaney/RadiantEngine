#pragma once
#include "rhi/rhi_context.h"
#include <memory>
#include <vector>
#include <optional>
#include <vulkan/vulkan.h>
#include <functional>

#pragma once
#include "rhi/rhi_context.h"
#include <memory>
#include <vector>
#include <optional>
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKQueue;
class RHIVKCommandBuffer;
class RHIVKFence;
class RHIVKSemaphore;



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
    RHICommandBuffer* create_command_buffer() override;
    RHIFence* create_fence() override;
    RHISemaphore* create_semaphore() override;
    RHISwapchain* create_swapchain(SDL_Window* window, uint32_t width, uint32_t height, uint32_t buffer_count) override;

    // Vulkan object accessors
    VkInstance get_vk_instance() const { return m_instance; }
    VkPhysicalDevice get_vk_physical_device() const { return m_physical_device; }
    VkDevice get_vk_device() const { return m_device; }
    VkQueue get_vk_graphics_queue() const { return m_graphics_queue; }
    uint32_t get_vk_graphics_queue_family() const { return m_graphics_queue_family; }
    VkCommandPool get_vk_command_pool() const { return m_command_pool; }

    // Set a custom validation callback (thread-unsafe, for test/dev only)
    static void set_validation_callback(ValidationCallback cb);

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

    std::unique_ptr<RHIVKQueue> m_rhi_graphics_queue;
};

} // namespace rhi::vulkan
