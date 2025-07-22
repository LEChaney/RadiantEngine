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
    VkInstance get_vk_instance() const { return instance_; }
    VkPhysicalDevice get_vk_physical_device() const { return physicalDevice_; }
    VkDevice get_vk_device() const { return device_; }
    VkQueue get_vk_graphics_queue() const { return graphicsQueue_; }
    uint32_t get_vk_graphics_queue_family() const { return graphicsQueueFamily_; }
    VkCommandPool get_vk_command_pool() const { return commandPool_; }

    // Set a custom validation callback (thread-unsafe, for test/dev only)
    static void set_validation_callback(ValidationCallback cb);

private:
    void create_instance();
    void pick_physical_device();
    void create_logical_device();
    void create_command_pool();
    void setup_debug_messenger();
    bool validation_enabled_ = false;

    VkInstance instance_{};
    VkPhysicalDevice physicalDevice_{};
    VkDevice device_{};
    VkQueue graphicsQueue_{};
    uint32_t graphicsQueueFamily_ = 0;
    VkCommandPool commandPool_{};
    VkDebugUtilsMessengerEXT debugMessenger_{};

    std::unique_ptr<RHIVKQueue> graphicsQueueWrapper_;
};

} // namespace rhi::vulkan
