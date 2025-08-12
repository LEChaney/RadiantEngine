#include "RHIVkContext.h"
#include "rhi/vulkan/queue/RHIVkQueue.h"
#include "rhi/vulkan/command/RHIVkCommandBuffer.h"
#include "rhi/vulkan/swapchain/RHIVkSwapchain.h"
#include "rhi/vulkan/buffer/RHIVkBuffer.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayoutBuilder.h"
#include "rhi/vulkan/pipeline/RHIVkPipelineLayoutBuilder.h"
#include "rhi/vulkan/pipeline/RHIVkShaderModule.h"
#include "rhi/vulkan/pipeline/RHIVkPipeline.h"
#include "rhi/vulkan/sync/RHIVkFence.h"
#include "rhi/vulkan/sync/RHIVkSemaphore.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/interface/swapchain/RHISwapchain.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorBufferArena.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorWriter.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "fmt/format.h"
#include <iostream>
#include <cstring>

namespace rhi::vulkan {

// Anonymous namespace for internal utilities
namespace {

// Global constant for default validation enable
#ifdef _DEBUG
constexpr bool gk_defaultEnableValidation = true;
#else
constexpr bool gk_defaultEnableValidation = false;
#endif
// Global constant for validation layers
const Array<const char*> gk_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Static state for validation error tracking
struct ValidationMsg {
    std::string message;
    RHIVkContext::ValidationLevel level;
};
// Global variable for validation errors
Array<ValidationMsg> g_validationErrors;
// Global variable for validation callback
rhi::vulkan::RHIVkContext::ValidationCallback g_validationCallback = nullptr;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*message_type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* /*user_data*/) 
{
    RHIVkContext::ValidationLevel level = RHIVkContext::ValidationLevel::Info;
    const char* levelStr = "INFO";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        level = RHIVkContext::ValidationLevel::Error;
        levelStr = "ERROR";
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        level = RHIVkContext::ValidationLevel::Warning;
        levelStr = "WARNING";
    }
    if (g_validationCallback) {
        g_validationCallback(callbackData->pMessage, level);
    } else {
        fmt::println(stderr, "[Vk Validation][{}] {}", levelStr, callbackData->pMessage);
    }
    return VK_FALSE;
}

bool checkValidationLayerSupport() {
    uint32 layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    Array<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
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

} // anonymous namespace

void RHIVkContext::setValidationCallback(ValidationCallback callback) {
    g_validationCallback = std::move(callback);
}

UniquePtr<RHIVkContext> RHIVkContext::createUnique(bool enableValidation) {
    return UniquePtr<RHIVkContext>(new RHIVkContext(enableValidation));
}

RHIVkContext::RHIVkContext(bool enableValidation)
    : m_validationEnabled(enableValidation || gk_defaultEnableValidation)
{
    if (volkInitialize() != VK_SUCCESS) {
        throw std::runtime_error("Failed to initialize Vulkan loader (volk)");
    }
    createInstance();
    volkLoadInstance(m_instance);
    setupDebugMessenger();
    pickPhysicalDevice();
    createLogicalDevice();
    volkLoadDevice(m_device);
    createCommandPool();
    queryDescriptorBufferProperties();
    createVmaAllocator();
}

RHIVkContext::~RHIVkContext() {
    if (m_vmaAllocator) {
        vmaDestroyAllocator(m_vmaAllocator);
        m_vmaAllocator = VK_NULL_HANDLE;
    }
    if (m_commandPool) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    }
    if (m_device) {
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_validationEnabled && m_debugMessenger) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) {
            func(m_instance, m_debugMessenger, nullptr);
        }
    }
    if (m_instance) {
        vkDestroyInstance(m_instance, nullptr);
    }
}

void RHIVkContext::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RHI Vulkan Test";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "RadiantEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3; // Request Vulkan 1.3

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    Array<const char*> enabledLayers;
    if (m_validationEnabled) {
        if (!checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested, but not available!");
        }
        enabledLayers = gk_validationLayers;
        createInfo.enabledLayerCount = static_cast<uint32>(enabledLayers.size());
        createInfo.ppEnabledLayerNames = enabledLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }
    
    // Always enable platform surface extensions
    Array<const char*> surfaceExts;
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
    Array<const char*> enabledExts;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    Array<VkExtensionProperties> availableExts(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExts.data());
    for (const auto& surfaceExt : surfaceExts) {
        if (std::ranges::find_if(availableExts, [&surfaceExt](const VkExtensionProperties& ext) {
                                     return strcmp(ext.extensionName, surfaceExt) == 0;
                                 }) != availableExts.end()) 
        {
            enabledExts.push_back(surfaceExt);
        }
    }
    // Enable debug utils if validation is enabled
    if (m_validationEnabled) {
        enabledExts.push_back("VK_EXT_debug_utils");
    }
    createInfo.enabledExtensionCount = static_cast<uint32>(enabledExts.size());
    createInfo.ppEnabledExtensionNames = enabledExts.empty() ? nullptr : enabledExts.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_validationEnabled) {
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
    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void RHIVkContext::setupDebugMessenger() {
    if (!m_validationEnabled) { 
        return; 
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (func && func(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

void RHIVkContext::queryDescriptorBufferProperties() {
    m_descBufferProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
    VkPhysicalDeviceProperties2 deviceProps{};
    deviceProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProps.pNext = &m_descBufferProps;
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &deviceProps);
}

void RHIVkContext::createVmaAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = m_instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT; // Enable buffer device address
    vmaCreateAllocator(&allocatorInfo, &m_vmaAllocator);
}

void RHIVkContext::pickPhysicalDevice() {
    uint32 deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan GPUs found");
    }
    Array<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    // Just pick the first device for now
    m_physicalDevice = devices[0];
}

void RHIVkContext::createLogicalDevice() {
    uint32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    Array<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
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

    Array<const char*> enabledLayers;
    if (m_validationEnabled) {
        enabledLayers = gk_validationLayers;
        createInfo.enabledLayerCount = static_cast<uint32>(enabledLayers.size());
        createInfo.ppEnabledLayerNames = enabledLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;
    }

    // Enable required device extensions
    Array<const char*> deviceExtensions = { 
        "VK_KHR_swapchain",
        "VK_KHR_buffer_device_address",
        "VK_EXT_descriptor_buffer",
        "VK_EXT_descriptor_indexing",
        "VK_KHR_synchronization2"
    };
    createInfo.enabledExtensionCount = static_cast<uint32>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Enable core Vulkan 1.2/1.3 features
    VkPhysicalDeviceVulkan12Features vulkan12{};
    vulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12.descriptorIndexing = VK_TRUE;
    vulkan12.bufferDeviceAddress = VK_TRUE;
#ifdef _DEBUG
    vulkan12.bufferDeviceAddressCaptureReplay = VK_TRUE;
#endif

    VkPhysicalDeviceVulkan13Features vulkan13{};
    vulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13.synchronization2 = VK_TRUE;

    // Enable descriptor buffer features
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures{};
    descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descriptorBufferFeatures.descriptorBuffer = VK_TRUE;
#ifdef _DEBUG
    descriptorBufferFeatures.descriptorBufferCaptureReplay = VK_TRUE;
#endif

    // Chain: descriptorBuffer -> vulkan13 -> vulkan12
    vulkan13.pNext = &vulkan12;
    descriptorBufferFeatures.pNext = &vulkan13;
    createInfo.pNext = &descriptorBufferFeatures;

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    m_rhiGraphicsQueue = RHIVkQueue::createUnique(this, graphicsQueueFamily);
}

void RHIVkContext::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_rhiGraphicsQueue->getVkQueueFamilyIndex();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }
}

RHIQueue* RHIVkContext::getGraphicsQueue() {
    return getVkGraphicsQueue();
}

RHIVkQueue* RHIVkContext::getVkGraphicsQueue() {
    return m_rhiGraphicsQueue.get();
}

UniquePtr<RHICommandBuffer> RHIVkContext::createCommandBuffer() {
    return createRhiVkCommandBuffer();
}

UniquePtr<RHIFence> RHIVkContext::createFence() {
    return createRhiVkFence();
}

UniquePtr<RHISemaphore> RHIVkContext::createSemaphore() {
    return createRhiVkSemaphore();
}

UniquePtr<RHIShaderModule> RHIVkContext::createShaderModule(const std::string& spvFilePath) {
    return createRhiVkShaderModule(spvFilePath);
}

UniquePtr<RHIShaderModule> RHIVkContext::createShaderModule(const Array<uint32>& shaderCode)
{
    return createRhiVkShaderModule(shaderCode);
}

UniquePtr<RHIPipeline> RHIVkContext::createComputePipeline(const RHIComputePipelineDescriptor& desc) {
    return createRhiVkComputePipeline(desc);
}

UniquePtr<RHIDescriptorBufferArena> RHIVkContext::createDescriptorBufferArena(const RHIDescriptorBufferArena::CreateInfo& createInfo)
{
    return createRhiVkDescriptorBufferArena(createInfo);
}

UniquePtr<RHIDescriptorWriter> RHIVkContext::createDescriptorWriter(const RHIDescriptorSetAllocation & alloc)
{
    return createRhiVkDescriptorWriter(alloc);
}

UniquePtr<RHIDescriptorSetLayoutBuilder> RHIVkContext::createDescriptorSetLayoutBuilder() {
    return createRhiVkDescriptorSetLayoutBuilder();
}

UniquePtr<RHIDescriptorSetBuilder> RHIVkContext::createDescriptorSetBuilder() {
    throw std::runtime_error("Descriptor set builder not implemented yet");
    //return createRhiVkDescriptorSetBuilder();
}

UniquePtr<RHIPipelineLayoutBuilder> RHIVkContext::createPipelineLayoutBuilder() {
    return createRhiVkPipelineLayoutBuilder();
}

UniquePtr<RHISwapchain> RHIVkContext::createSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 imageCount, RHIImageUsageFlags extraImageUsage) {
    return createRhiVkSwapchain(window, width, height, imageCount, extraImageUsage);
}

UniquePtr<RHIBuffer> RHIVkContext::createBuffer(uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) {
    return createRhiVkBuffer(size, usage, memProps);
}

UniquePtr<RHIImage> RHIVkContext::createImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage, RHIMemoryPropertyFlags memProps) {
    return createRhiVkImage(width, height, format, usage, memProps);
}

UniquePtr<RHIVkCommandBuffer> RHIVkContext::createRhiVkCommandBuffer() {
    return RHIVkCommandBuffer::createUnique(this);
}

UniquePtr<RHIVkFence> RHIVkContext::createRhiVkFence() {
    return RHIVkFence::createUnique(this);
}

UniquePtr<RHIVkSemaphore> RHIVkContext::createRhiVkSemaphore() {
    return RHIVkSemaphore::createUnique(this);
}

UniquePtr<RHIVkSwapchain> RHIVkContext::createRhiVkSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 imageCount, RHIImageUsageFlags extraImageUsage) {
    return RHIVkSwapchain::createUnique(this, window, width, height, imageCount, extraImageUsage);
}

UniquePtr<RHIVkBuffer> RHIVkContext::createRhiVkBuffer(uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) {
    return RHIVkBuffer::createUnique(this, size, usage, memProps);
}

UniquePtr<RHIVkImage> RHIVkContext::createRhiVkImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage, RHIMemoryPropertyFlags memProps) {
    return RHIVkImage::createUnique(this, width, height, format, usage, memProps);
}

UniquePtr<RHIVkDescriptorSetLayoutBuilder> RHIVkContext::createRhiVkDescriptorSetLayoutBuilder() {
    return RHIVkDescriptorSetLayoutBuilder::createUnique(this);
}

UniquePtr<RHIVkDescriptorSetBuilder> RHIVkContext::createRhiVkDescriptorSetBuilder() {
    throw std::runtime_error("Descriptor set builder not implemented yet");
    //return RHIVkDescriptorSetBuilder::createUnique(this);
}

UniquePtr<RHIVkPipelineLayoutBuilder> RHIVkContext::createRhiVkPipelineLayoutBuilder()
{
    return RHIVkPipelineLayoutBuilder::createUnique(this);
}

UniquePtr<RHIVkShaderModule> RHIVkContext::createRhiVkShaderModule(
    const std::string& spvFilePath) 
{
    return RHIVkShaderModule::createUnique(this, spvFilePath);
}

UniquePtr<RHIVkShaderModule> RHIVkContext::createRhiVkShaderModule(const Array<uint32>& shaderCode)
{
    return RHIVkShaderModule::createUnique(this, shaderCode);
}

UniquePtr<RHIVkPipeline> RHIVkContext::createRhiVkComputePipeline(
    const RHIComputePipelineDescriptor& desc) 
{
    return RHIVkPipeline::createUniqueCompute(this, desc);
}

UniquePtr<RHIVkDescriptorBufferArena> RHIVkContext::createRhiVkDescriptorBufferArena(const RHIDescriptorBufferArena::CreateInfo& createInfo)
{
    return RHIVkDescriptorBufferArena::createUnique(this, createInfo);
}

UniquePtr<RHIDescriptorWriter> RHIVkContext::createRhiVkDescriptorWriter(const RHIDescriptorSetAllocation& alloc) {
    return RHIVkDescriptorWriter::createUnique(this, alloc);
}



} // namespace rhi::vulkan
