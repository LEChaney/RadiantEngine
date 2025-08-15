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
#include "rhi/vulkan/descriptor/RHIVkDescriptorBuffer.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorWriter.h"
#include "rhi/interface/swapchain/RHISwapchain.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "fmt/format.h"
#include <iostream>
#include <cstring>
#include <ranges>

namespace RHI::Vulkan {

// Anonymous namespace for internal utilities
namespace {

// Global constant for default validation mode
#ifdef _DEBUG
constexpr RHIVkContext::ValidationMode gk_defaultValidationMode = RHIVkContext::ValidationMode::Standard;
#else
constexpr RHIVkContext::ValidationMode gk_defaultValidationMode = RHIVkContext::ValidationMode::None;
#endif
// Global constant for validation layers
const Array<const char*> gk_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// Global variable for validation callback
RHIVkContext::ValidationCallback g_validationCallback = nullptr;

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

UniquePtr<RHIVkContext> RHIVkContext::createUnique(ValidationMode mode) {
    return UniquePtr<RHIVkContext>(new RHIVkContext(mode));
}

RHIVkContext::RHIVkContext(ValidationMode mode)
    : m_validationMode(mode == ValidationMode::Auto ? gk_defaultValidationMode : mode)
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
    if (m_validationMode != ValidationMode::None && m_debugMessenger) {
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

    const bool enableValidation = (m_validationMode != ValidationMode::None);

    Array<const char*> enabledLayers;
    if (enableValidation) {
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

    // Query available instance extensions
    uint32 extCount = 0;
    Array<const char*> enabledExts;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    Array<VkExtensionProperties> availableExts(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, availableExts.data());
    auto hasInstExt = [&availableExts](const char* name) {
        return std::ranges::find_if(availableExts, [name](const VkExtensionProperties& ext) {
                   return strcmp(ext.extensionName, name) == 0;
               }) != availableExts.end();
    };
    for (const auto& surfaceExt : surfaceExts) {
        if (hasInstExt(surfaceExt)) {
            enabledExts.push_back(surfaceExt);
        }
    }
    // Enable debug utils if validation is enabled
    if (enableValidation && hasInstExt("VK_EXT_debug_utils")) {
        enabledExts.push_back("VK_EXT_debug_utils");
    }
    // Enable validation features extension if GPU-assisted is requested
    const bool useGpuAv = (m_validationMode == ValidationMode::GpuAssisted);
    if (enableValidation && useGpuAv) {
        enabledExts.push_back("VK_EXT_validation_features");
    }
    createInfo.enabledExtensionCount = static_cast<uint32>(enabledExts.size());
    createInfo.ppEnabledExtensionNames = enabledExts.empty() ? nullptr : enabledExts.data();

    // Instance pNext chain
    VkValidationFeaturesEXT validationFeatures{};
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    void* pNextChain = nullptr;

    if (enableValidation) {
        // Optionally add validation features for GPU-AV
        if (useGpuAv) {
            static const VkValidationFeatureEnableEXT enables[] = {
                VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
                VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT
            };
            static const VkValidationFeatureDisableEXT disables[] = {
                // Disable core CPU checks to avoid double validation/slowness when using GPU-AV only
                VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT
            };
            validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            validationFeatures.pNext = nullptr;
            validationFeatures.enabledValidationFeatureCount = static_cast<uint32>(std::size(enables));
            validationFeatures.pEnabledValidationFeatures = enables;
            validationFeatures.disabledValidationFeatureCount = static_cast<uint32>(std::size(disables));
            validationFeatures.pDisabledValidationFeatures = disables;
            pNextChain = &validationFeatures;
        }

        // Chain initial debug messenger for early messages
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        debugCreateInfo.pNext = pNextChain;
        pNextChain = &debugCreateInfo;
    }

    createInfo.pNext = pNextChain;

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance");
    }
}

void RHIVkContext::setupDebugMessenger() {
    if (m_validationMode == ValidationMode::None) { 
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

    const bool enableValidation = (m_validationMode != ValidationMode::None);

    Array<const char*> enabledLayers;
    if (enableValidation) {
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

UniquePtr<RHIShaderModule> RHIVkContext::createShaderModule(const Path& spvFilePath) {
    return createRhiVkShaderModule(spvFilePath);
}

UniquePtr<RHIShaderModule> RHIVkContext::createShaderModule(const Array<uint32>& shaderCode)
{
    return createRhiVkShaderModule(shaderCode);
}

UniquePtr<RHIPipeline> RHIVkContext::createComputePipeline(const RHIComputePipelineDescriptor& desc) {
    return createRhiVkComputePipeline(desc);
}

UniquePtr<RHIDescriptorBuffer> RHIVkContext::createDescriptorBuffer(const RHIDescriptorBufferCreateInfo& createInfo)
{
    return createRhiVkDescriptorBuffer(createInfo);
}

const RHIDescriptorWriterOps* RHIVkContext::getDescriptorWriterOps() const {
    return getVkDescriptorWriterOps();
}

const RHIDescriptorSetLayoutBuilderOps* RHIVkContext::getDescriptorSetLayoutBuilderOps() const {
    return getVkDescriptorSetLayoutBuilderOps();
}
const RHIPipelineLayoutBuilderOps* RHIVkContext::getPipelineLayoutBuilderOps() const {
    return getVkPipelineLayoutBuilderOps();
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

UniquePtr<RHIVkShaderModule> RHIVkContext::createRhiVkShaderModule(const Path& spvFilePath)
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

UniquePtr<RHIVkDescriptorBuffer> RHIVkContext::createRhiVkDescriptorBuffer(const RHIDescriptorBufferCreateInfo& createInfo)
{
    return RHIVkDescriptorBuffer::createUnique(this, createInfo);
}

} // namespace rhi::vulkan
