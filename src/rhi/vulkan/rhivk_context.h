
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

    Queue* get_graphics_queue() override;
    CommandBuffer* create_command_buffer() override;
    Fence* create_fence() override;
    Semaphore* create_semaphore() override;

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
