#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_queue.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_swapchain.h"
#include "rhi/vulkan/rhivk_buffer.h"
#include "rhi/vulkan/rhivk_fence.h"
#include "rhi/vulkan/rhivk_semaphore.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include "rhi/rhi_swapchain.h"
#include "rhivk_context.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "fmt/format.h"
#include <iostream>
#include <cstring>

namespace rhi::vulkan {

namespace {
// Global constant for default validation enable
#ifdef _DEBUG
constexpr bool gk_defaultEnableValidation = true;
#else
constexpr bool gk_defaultEnableValidation = false;
#endif
// Global constant for validation layers
const std::vector<const char*> gk_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Static state for validation error tracking
struct ValidationMsg {
    std::string message;
    RHIVKContext::ValidationLevel level;
};
// Global variable for validation errors
std::vector<ValidationMsg> g_validationErrors;
// Global variable for validation callback
rhi::vulkan::RHIVKContext::ValidationCallback g_validationCallback = nullptr;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*message_type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* /*user_data*/) 
{
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
        g_validationCallback(callbackData->pMessage, level);
    } else {
        std::cerr << fmt::format("[Vk Validation][{}] {}", levelStr, callbackData->pMessage) << std::endl;
    }
    return VK_FALSE;
}

bool checkValidationLayerSupport() {
    uint32 layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> availableLayers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, availableLayers.data());
    for (const char* layerName : gk_validationLayers) {
        bool found = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
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
void RHIVKContext::setValidationCallback(ValidationCallback callback) {
    g_validationCallback = std::move(callback);
}

UniquePtr<RHIVKContext> RHIVKContext::createUnique(bool enableValidation) {
    return UniquePtr<RHIVKContext>(new RHIVKContext(enableValidation));
}

RHIVKContext::RHIVKContext(bool enableValidation)
    : m_validationEnabled(enableValidation || gk_defaultEnableValidation)
{
    createInstance();
    setupDebugMessenger();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();

    // Create VMA allocator
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;
    vmaCreateAllocator(&allocatorInfo, &m_vmaAllocator);
}

RHIVKContext::~RHIVKContext() {
    if (m_vmaAllocator) {
        vmaDestroyAllocator(m_vmaAllocator);
        m_vmaAllocator = VK_NULL_HANDLE;
    }
    if (m_commandPool) vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    if (m_device) vkDestroyDevice(m_device, nullptr);
    if (m_validationEnabled && m_debugMessenger) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(m_instance, m_debugMessenger, nullptr);
    }
    if (m_instance) vkDestroyInstance(m_instance, nullptr);
}

void RHIVKContext::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RHI Vulkan Test";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "RadiantEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &appInfo;

    std::vector<const char*> enabledLayers;
    if (m_validationEnabled) {
        if (!checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }
        enabledLayers = gk_validationLayers;
        create_info.enabledLayerCount = static_cast<uint32>(enabledLayers.size());
        create_info.ppEnabledLayerNames = enabledLayers.data();
    } else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
    }
    
    // Always enable platform surface extensions
    std::vector<const char*> surfaceExts;
    surfaceExts.push_back("VK_KHR_surface");
#ifdef _WIN32
    surfaceExts.push_back("VK_KHR_win32_surface");
#elif defined(__linux__)
    surfaceExts.push_back("VK_KHR_xcb_surface");
    surfaceExts.push_back("VK_KHR_xlib_surface");
    surfaceExts.push_back("VK_KHR_wayland_surface");
#elif defined(__APPLE__)
    surfaceExts.push_back("VK_EXT_metal_surface");
#elif defined(__ANDROID__)
    surfaceExts.push_back("VK_KHR_android_surface");
#endif

    // Query required instance extensions
    uint32 extCount = 0;
    std::vector<const char*> enabledExts;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> availableExts(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExts.data());
    for (const auto& surfaceExt : surfaceExts) {
        if (std::find_if(availableExts.begin(), availableExts.end(),
                         [&surfaceExt](const VkExtensionProperties& ext) {
                             return strcmp(ext.extensionName, surfaceExt) == 0;
                         }) != availableExts.end()) {
            enabledExts.push_back(surfaceExt);
        }
    }
    // Enable debug utils if validation is enabled
    if (m_validationEnabled) {
        enabledExts.push_back("VK_EXT_debug_utils");
    }
    create_info.enabledExtensionCount = static_cast<uint32>(enabledExts.size());
    create_info.ppEnabledExtensionNames = enabledExts.empty() ? nullptr : enabledExts.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_validationEnabled) {
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        create_info.pNext = &debugCreateInfo;
    } else {
        create_info.pNext = nullptr;
    }
    if (vkCreateInstance(&create_info, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void RHIVKContext::setupDebugMessenger() {
    if (!m_validationEnabled) return;
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    if (func && func(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

void RHIVKContext::pickPhysicalDevice() {
    uint32 deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("No Vulkan GPUs found");
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    // Just pick the first device for now
    m_physicalDevice = devices[0];
}

void RHIVKContext::createLogicalDevice() {
    uint32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());
    uint32 graphicsQueueFamily = 0;
    for (uint32 i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsQueueFamily = i;
            break;
        }
    }
    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;

    std::vector<const char*> enabledLayers;
    if (m_validationEnabled) {
        enabledLayers = gk_validationLayers;
        createInfo.enabledLayerCount = static_cast<uint32>(enabledLayers.size());
        createInfo.ppEnabledLayerNames = enabledLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    // Enable VK_KHR_swapchain device extension
    std::vector<const char*> deviceExtensions = { "VK_KHR_swapchain" };
    createInfo.enabledExtensionCount = static_cast<uint32>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }
    m_rhiGraphicsQueue = RHIVKQueue::createUnique(this, graphicsQueueFamily);
}

void RHIVKContext::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_rhiGraphicsQueue->getVkQueueFamilyIndex();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

RHIQueue* RHIVKContext::getGraphicsQueue() {
    return getVkGraphicsQueue();
}

RHIVKQueue* RHIVKContext::getVkGraphicsQueue() {
    return m_rhiGraphicsQueue.get();
}

UniquePtr<RHICommandBuffer> RHIVKContext::createCommandBuffer() {
    return createRhiVkCommandBuffer();
}

UniquePtr<RHIFence> RHIVKContext::createFence() {
    return createRhiVkFence();
}

UniquePtr<RHISemaphore> RHIVKContext::createSemaphore() {
    return createRhiVkSemaphore();
}

UniquePtr<RHISwapchain> RHIVKContext::createSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 imageCount) {
    return createRhiVkSwapchain(window, width, height, imageCount);
}

UniquePtr<RHIBuffer> RHIVKContext::createBuffer(uint64 size, RHIBufferUsage usage, RHIMemoryProperty memProps) {
    return createRhiVkBuffer(size, usage, memProps);
}

UniquePtr<RHIImage> RHIVKContext::createImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsage usage, RHIMemoryProperty memProps) {
    return createRhiVkImage(width, height, format, usage, memProps);
}

UniquePtr<RHIVKCommandBuffer> RHIVKContext::createRhiVkCommandBuffer() {
    return RHIVKCommandBuffer::createUnique(this);
}

UniquePtr<RHIVKFence> RHIVKContext::createRhiVkFence() {
    return RHIVKFence::createUnique(this);
}

UniquePtr<RHIVKSemaphore> RHIVKContext::createRhiVkSemaphore() {
    return RHIVKSemaphore::createUnique(this);
}

UniquePtr<RHIVKSwapchain> RHIVKContext::createRhiVkSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 imageCount) {
    return RHIVKSwapchain::createUnique(this, window, width, height, imageCount);
}

UniquePtr<RHIVKBuffer> RHIVKContext::createRhiVkBuffer(uint64 size, RHIBufferUsage usage, RHIMemoryProperty memProps) {
    return RHIVKBuffer::createUnique(this, size, usage, memProps);
}

UniquePtr<RHIVKImage> RHIVKContext::createRhiVkImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsage usage, RHIMemoryProperty memProps) {
    return RHIVKImage::createUnique(this, width, height, format, usage, memProps);
}

} // namespace rhi::vulkan
