#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_queue.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_swapchain.h"
#include "fmt/format.h"
#include <iostream>
#include <cstring>

namespace rhi::vulkan {

namespace {
#ifdef _DEBUG
constexpr bool k_default_enable_validation = true;
#else
constexpr bool k_default_enable_validation = false;
#endif
const std::vector<const char*> validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};

// Static state for validation error tracking
struct ValidationMsg {
    std::string message;
    RHIVKContext::ValidationLevel level;
};
static std::vector<ValidationMsg> g_validation_errors;
static rhi::vulkan::RHIVKContext::ValidationCallback g_validation_callback = nullptr;

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT /*message_type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/) 
{
    RHIVKContext::ValidationLevel level = RHIVKContext::ValidationLevel::Info;

    const char* level_str = "INFO";
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        level = RHIVKContext::ValidationLevel::Error;
        level_str = "ERROR";
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        level = RHIVKContext::ValidationLevel::Warning;
        level_str = "WARNING";
    }

    if (g_validation_callback) {
        g_validation_callback(callback_data->pMessage, level);
    } else {
        std::cerr << fmt::format("[Vk Validation][{}] {}", level_str, callback_data->pMessage) << std::endl;
    }
    return VK_FALSE;
}

bool check_validation_layer_support() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* layer_name : validation_layers) {
        bool found = false;
        for (const auto& layer_properties : available_layers) {
            if (strcmp(layer_name, layer_properties.layerName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }
    }

    return true;
}
}
void RHIVKContext::set_validation_callback(ValidationCallback callback) {
    g_validation_callback = std::move(callback);
}

RHIVKContext::RHIVKContext(bool enable_validation)
    : validation_enabled_(enable_validation || k_default_enable_validation) {
    create_instance();
    setup_debug_messenger();
    pick_physical_device();
    create_logical_device();
    create_command_pool();
    m_rhi_graphics_queue = std::make_unique<RHIVKQueue>(m_graphics_queue, m_device);
}

RHIVKContext::~RHIVKContext() {
    if (m_command_pool) vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    if (m_device) vkDestroyDevice(m_device, nullptr);
    if (validation_enabled_ && m_debug_messenger) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(m_instance, m_debug_messenger, nullptr);
    }
    if (m_instance) vkDestroyInstance(m_instance, nullptr);
}

void RHIVKContext::create_instance() {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "RHI Vulkan Test";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "RadiantEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    std::vector<const char*> enabled_layers;
    if (validation_enabled_) {
        if (!check_validation_layer_support()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }
        enabled_layers = validation_layers;
        create_info.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
        create_info.ppEnabledLayerNames = enabled_layers.data();
    } else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
    }
    
    // Always enable platform surface extensions
    std::vector<const char*> surface_exts;
    surface_exts.push_back("VK_KHR_surface");
#ifdef _WIN32
    surface_exts.push_back("VK_KHR_win32_surface");
#elif defined(__linux__)
    surface_exts.push_back("VK_KHR_xcb_surface");
    surface_exts.push_back("VK_KHR_xlib_surface");
    surface_exts.push_back("VK_KHR_wayland_surface");
#elif defined(__APPLE__)
    surface_exts.push_back("VK_EXT_metal_surface");
#elif defined(__ANDROID__)
    surface_exts.push_back("VK_KHR_android_surface");
#endif

    // Query required instance extensions
    uint32_t ext_count = 0;
    std::vector<const char*> enabled_exts;
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> available_exts(ext_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, available_exts.data());
    for (const auto& surface_ext : surface_exts) {
        if (std::find_if(available_exts.begin(), available_exts.end(),
                         [&surface_ext](const VkExtensionProperties& ext) {
                             return strcmp(ext.extensionName, surface_ext) == 0;
                         }) != available_exts.end()) {
            enabled_exts.push_back(surface_ext);
        }
    }

    // Enable debug utils if validation is enabled
    if (validation_enabled_) {
        enabled_exts.push_back("VK_EXT_debug_utils");
    }

    create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_exts.size());
    create_info.ppEnabledExtensionNames = enabled_exts.empty() ? nullptr : enabled_exts.data();

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    if (validation_enabled_) {
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = debug_callback;
        create_info.pNext = &debug_create_info;
    } else {
        create_info.pNext = nullptr;
    }

    if (vkCreateInstance(&create_info, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void RHIVKContext::setup_debug_messenger() {
    if (!validation_enabled_) return;
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if (func && func(m_instance, &create_info, nullptr, &m_debug_messenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

void RHIVKContext::pick_physical_device() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
    if (device_count == 0) throw std::runtime_error("No Vulkan GPUs found");
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());
    // Just pick the first device for now
    m_physical_device = devices[0];
}

void RHIVKContext::create_logical_device() {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, queue_families.data());
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_graphics_queue_family = i;
            break;
        }
    }
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = m_graphics_queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_create_info;

    std::vector<const char*> enabled_layers;
    if (validation_enabled_) {
        enabled_layers = validation_layers;
        create_info.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
        create_info.ppEnabledLayerNames = enabled_layers.data();
    } else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
    }

    // Enable VK_KHR_swapchain device extension
    std::vector<const char*> device_extensions = { "VK_KHR_swapchain" };
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();

    if (vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }
    vkGetDeviceQueue(m_device, m_graphics_queue_family, 0, &m_graphics_queue);
}

void RHIVKContext::create_command_pool() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = m_graphics_queue_family;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

RHIQueue* RHIVKContext::get_graphics_queue() {
    return m_rhi_graphics_queue.get();
}

RHICommandBuffer* RHIVKContext::create_command_buffer() {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd_buffer;
    if (vkAllocateCommandBuffers(m_device, &alloc_info, &cmd_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }
    return new RHIVKCommandBuffer(cmd_buffer, m_device, m_command_pool);
}

RHIFence* RHIVKContext::create_fence() {
    // Not needed for minimal test
    return nullptr;
}

RHISemaphore* RHIVKContext::create_semaphore() {
    // Not needed for minimal test
    return nullptr;
}

RHISwapchain* RHIVKContext::create_swapchain(SDL_Window* window, uint32_t width, uint32_t height, uint32_t image_count) {
    return new RHIVKSwapchain(this, window, width, height, image_count);
}

} // namespace rhi::vulkan
