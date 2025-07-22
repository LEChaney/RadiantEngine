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
constexpr bool kDefaultEnableValidation = true;
#else
constexpr bool kDefaultEnableValidation = false;
#endif
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Static state for validation error tracking
struct ValidationMsg {
    std::string message;
    RHIVKContext::ValidationLevel level;
};
static std::vector<ValidationMsg> g_validationErrors;
static rhi::vulkan::RHIVKContext::ValidationCallback g_validationCallback = nullptr;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/) {
    RHIVKContext::ValidationLevel level = RHIVKContext::ValidationLevel::Info;
    const char* levelStr = "INFO";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        level = RHIVKContext::ValidationLevel::Error;
        levelStr = "ERROR";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        level = RHIVKContext::ValidationLevel::Warning;
        levelStr = "WARNING";
    }
    if (g_validationCallback) {
        g_validationCallback(pCallbackData->pMessage, level);
    } else {
        std::cerr << fmt::format("[Vk Validation][{}] {}", levelStr, pCallbackData->pMessage) << std::endl;
    }
    return VK_FALSE;
}

bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    for (const char* layerName : validationLayers) {
        bool found = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}
}
void RHIVKContext::set_validation_callback(ValidationCallback cb) {
    g_validationCallback = std::move(cb);
}

RHIVKContext::RHIVKContext(bool enableValidation)
    : validation_enabled_(enableValidation || kDefaultEnableValidation) {
    create_instance();
    setup_debug_messenger();
    pick_physical_device();
    create_logical_device();
    create_command_pool();
    graphicsQueueWrapper_ = std::make_unique<RHIVKQueue>(graphicsQueue_, device_);
}

RHIVKContext::~RHIVKContext() {
    if (commandPool_) vkDestroyCommandPool(device_, commandPool_, nullptr);
    if (device_) vkDestroyDevice(device_, nullptr);
    if (validation_enabled_ && debugMessenger_) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(instance_, debugMessenger_, nullptr);
    }
    if (instance_) vkDestroyInstance(instance_, nullptr);
}

void RHIVKContext::create_instance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RHI Vulkan Test";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "RadiantEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> enabledLayers;
    if (validation_enabled_) {
        if (!checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }
        enabledLayers = validationLayers;
        createInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
        createInfo.ppEnabledLayerNames = enabledLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
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

    createInfo.enabledExtensionCount = static_cast<uint32_t>(enabled_exts.size());
    createInfo.ppEnabledExtensionNames = enabled_exts.empty() ? nullptr : enabled_exts.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (validation_enabled_) {
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void RHIVKContext::setup_debug_messenger() {
    if (!validation_enabled_) return;
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT");
    if (func && func(instance_, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

void RHIVKContext::pick_physical_device() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("No Vulkan GPUs found");
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
    // Just pick the first device for now
    physicalDevice_ = devices[0];
}

void RHIVKContext::create_logical_device() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, queueFamilies.data());
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsQueueFamily_ = i;
            break;
        }
    }
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily_;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;

    std::vector<const char*> enabledLayers;
    if (validation_enabled_) {
        enabledLayers = validationLayers;
        createInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
        createInfo.ppEnabledLayerNames = enabledLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    // Enable VK_KHR_swapchain device extension
    std::vector<const char*> deviceExtensions = { "VK_KHR_swapchain" };
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }
    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
}

void RHIVKContext::create_command_pool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = graphicsQueueFamily_;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

RHIQueue* RHIVKContext::get_graphics_queue() {
    return graphicsQueueWrapper_.get();
}

RHICommandBuffer* RHIVKContext::create_command_buffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmdBuffer;
    if (vkAllocateCommandBuffers(device_, &allocInfo, &cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }
    return new RHIVKCommandBuffer(cmdBuffer, device_, commandPool_);
}

RHIFence* RHIVKContext::create_fence() {
    // Not needed for minimal test
    return nullptr;
}

RHISemaphore* RHIVKContext::create_semaphore() {
    // Not needed for minimal test
    return nullptr;
}

RHISwapchain* RHIVKContext::create_swapchain(SDL_Window* window, uint32_t width, uint32_t height, uint32_t buffer_count) {
    return new RHIVKSwapchain(this, window, width, height, buffer_count);
}

} // namespace rhi::vulkan
