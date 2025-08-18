#include "RHIVkContext.h"
#include "rhi/vulkan/queue/RHIVkQueue.h"
#include "rhi/vulkan/command/RHIVkCommandBuffer.h"
#include "rhi/vulkan/swapchain/RHIVkSwapchain.h"
#include "rhi/vulkan/buffer/RHIVkBuffer.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"
#include "rhi/vulkan/pipeline/RHIVkPipelineLayout.h"
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
#include <array>

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
            static constexpr std::array<VkValidationFeatureEnableEXT,2> enables = {
                VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
                VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT
            };
            static constexpr std::array<VkValidationFeatureDisableEXT,1> disables = {
                // Disable core CPU checks to avoid double validation/slowness when using GPU-AV only
                VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT
            };
            validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            validationFeatures.pNext = nullptr;
            validationFeatures.enabledValidationFeatureCount = static_cast<uint32>(enables.size());
            validationFeatures.pEnabledValidationFeatures = enables.data();
            validationFeatures.disabledValidationFeatureCount = static_cast<uint32>(disables.size());
            validationFeatures.pDisabledValidationFeatures = disables.data();
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

uint32 RHIVkContext::findGraphicsQueueFamily() const {
    uint32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, nullptr);
    Array<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, props.data());
    for (uint32 i = 0; i < count; ++i) {
        if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }
    throw std::runtime_error("No graphics queue family found");
}

bool RHIVkContext::gatherDeviceExtensions(Array<const char*>& deviceExtensions) const {
    deviceExtensions.clear();
    deviceExtensions.push_back("VK_KHR_swapchain");
    deviceExtensions.push_back("VK_KHR_buffer_device_address");
    deviceExtensions.push_back("VK_EXT_descriptor_buffer");
    deviceExtensions.push_back("VK_EXT_descriptor_indexing");
    deviceExtensions.push_back("VK_KHR_synchronization2");
    uint32 devExtCount = 0;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &devExtCount, nullptr);
    Array<VkExtensionProperties> devExts(devExtCount);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &devExtCount, devExts.data());
    auto hasExt = [&devExts](const char* name) -> bool {
        for (const auto& e : devExts) {
            if (strcmp(e.extensionName, name) == 0) {
                return true;
            }
        }
        return false;
    };
    bool hasDeferredHost = hasExt("VK_KHR_deferred_host_operations");
    bool hasAccel = hasExt("VK_KHR_acceleration_structure");
    bool hasRtPipeline = hasExt("VK_KHR_ray_tracing_pipeline");
    bool hasRayQueryExt = hasExt("VK_KHR_ray_query");
    bool hasMeshShader = hasExt("VK_EXT_mesh_shader");
    if (!(hasDeferredHost && hasAccel && hasRtPipeline)) {
        throw std::runtime_error("Required ray tracing extensions not supported by selected GPU");
    }
    if (!hasMeshShader) {
        throw std::runtime_error("Required mesh shader extension VK_EXT_mesh_shader not supported by selected GPU");
    }
    deviceExtensions.push_back("VK_KHR_deferred_host_operations");
    deviceExtensions.push_back("VK_KHR_acceleration_structure");
    deviceExtensions.push_back("VK_KHR_ray_tracing_pipeline");
    deviceExtensions.push_back("VK_EXT_mesh_shader");
    if (hasRayQueryExt) {
        deviceExtensions.push_back("VK_KHR_ray_query");
        return true;
    }
    return false;
}


void RHIVkContext::createLogicalDevice() {
    uint32 graphicsQueueFamily = findGraphicsQueueFamily();
    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCi{};
    queueCi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCi.queueFamilyIndex = graphicsQueueFamily;
    queueCi.queueCount = 1;
    queueCi.pQueuePriorities = &queuePriority;
    VkDeviceCreateInfo deviceCi{};
    deviceCi.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCi.queueCreateInfoCount = 1;
    deviceCi.pQueueCreateInfos = &queueCi;
    const bool enableValidation = (m_validationMode != ValidationMode::None);
    Array<const char*> enabledLayers;
    if (enableValidation) {
        enabledLayers = gk_validationLayers;
        deviceCi.enabledLayerCount = static_cast<uint32>(enabledLayers.size());
        deviceCi.ppEnabledLayerNames = enabledLayers.data();
    }
    Array<const char*> deviceExtensions;
    bool enableRayQuery = gatherDeviceExtensions(deviceExtensions);
    deviceCi.enabledExtensionCount = static_cast<uint32>(deviceExtensions.size());
    deviceCi.ppEnabledExtensionNames = deviceExtensions.data();
    // Query feature chain
    // NOTE: Feature query & enabling previously handled by buildFeatureChains; now inlined to reduce indirection and avoid dangling pNext pointers.
    VkPhysicalDeviceFeatures2 queryFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceDescriptorBufferFeaturesEXT queryDescBuf{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
    VkPhysicalDeviceVulkan13Features queryVk13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    VkPhysicalDeviceVulkan12Features queryVk12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceMeshShaderFeaturesEXT queryMesh{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR queryAccel{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR queryRtPipe{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
    VkPhysicalDeviceRayQueryFeaturesKHR queryRayQuery{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
    queryFeatures2.pNext = &queryDescBuf;
    queryDescBuf.pNext = &queryVk13;
    queryVk13.pNext = &queryVk12;
    queryVk12.pNext = &queryMesh;
    queryMesh.pNext = &queryAccel;
    queryAccel.pNext = &queryRtPipe;
    if (enableRayQuery) {
        queryRtPipe.pNext = &queryRayQuery;
    }
    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &queryFeatures2);
    // Enable feature chain (lifetime valid until device creation finishes)
    VkPhysicalDeviceFeatures2 enableFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceDescriptorBufferFeaturesEXT enableDescBuf{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT };
    VkPhysicalDeviceVulkan13Features enableVk13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    VkPhysicalDeviceVulkan12Features enableVk12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceMeshShaderFeaturesEXT enableMesh{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR enableAccel{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR enableRtPipe{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
    VkPhysicalDeviceRayQueryFeaturesKHR enableRayQueryFeat{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
    enableFeatures2.pNext = &enableDescBuf;
    enableDescBuf.pNext = &enableVk13;
    enableVk13.pNext = &enableVk12;
    enableVk12.pNext = &enableMesh;
    enableMesh.pNext = &enableAccel;
    enableAccel.pNext = &enableRtPipe;
    if (enableRayQuery) {
        enableRtPipe.pNext = &enableRayQueryFeat;
    }
    // Core optional features
    if (queryFeatures2.features.fragmentStoresAndAtomics) {
        enableFeatures2.features.fragmentStoresAndAtomics = VK_TRUE;
    }
    if (queryFeatures2.features.vertexPipelineStoresAndAtomics) {
        enableFeatures2.features.vertexPipelineStoresAndAtomics = VK_TRUE;
    }
    if (queryFeatures2.features.shaderInt64) {
        enableFeatures2.features.shaderInt64 = VK_TRUE;
    }
    // Vulkan 1.2
    if (queryVk12.descriptorIndexing) {
        enableVk12.descriptorIndexing = VK_TRUE;
    }
    if (queryVk12.bufferDeviceAddress) {
        enableVk12.bufferDeviceAddress = VK_TRUE;
    }
    if (queryVk12.bufferDeviceAddressCaptureReplay) {
        enableVk12.bufferDeviceAddressCaptureReplay = VK_TRUE;
    }
    if (queryVk12.storageBuffer8BitAccess) {
        enableVk12.storageBuffer8BitAccess = VK_TRUE;
    }
    if (queryVk12.uniformAndStorageBuffer8BitAccess) {
        enableVk12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    }
    if (queryVk12.storagePushConstant8) {
        enableVk12.storagePushConstant8 = VK_TRUE;
    }
    if (queryVk12.timelineSemaphore) {
        enableVk12.timelineSemaphore = VK_TRUE;
    }
    if (queryVk12.vulkanMemoryModel) {
        enableVk12.vulkanMemoryModel = VK_TRUE;
    }
    if (queryVk12.vulkanMemoryModelDeviceScope) {
        enableVk12.vulkanMemoryModelDeviceScope = VK_TRUE;
    }
    // Vulkan 1.3
    if (queryVk13.synchronization2) {
        enableVk13.synchronization2 = VK_TRUE;
    }
    if (queryVk13.dynamicRendering) {
        enableVk13.dynamicRendering = VK_TRUE;
    }
    if (queryVk13.maintenance4) {
        enableVk13.maintenance4 = VK_TRUE;
    }
    // Mesh shader
    if (queryMesh.meshShader) {
        enableMesh.meshShader = VK_TRUE;
    }
    if (queryMesh.taskShader) {
        enableMesh.taskShader = VK_TRUE;
    }
    // Descriptor buffer
    if (queryDescBuf.descriptorBuffer) {
        enableDescBuf.descriptorBuffer = VK_TRUE;
    }
    if (queryDescBuf.descriptorBufferCaptureReplay) {
        enableDescBuf.descriptorBufferCaptureReplay = VK_TRUE;
    }
    // Ray tracing / query
    if (queryAccel.accelerationStructure) {
        enableAccel.accelerationStructure = VK_TRUE;
    }
    if (queryRtPipe.rayTracingPipeline) {
        enableRtPipe.rayTracingPipeline = VK_TRUE;
    }
    if (queryRtPipe.rayTracingPipelineTraceRaysIndirect) {
        enableRtPipe.rayTracingPipelineTraceRaysIndirect = VK_TRUE;
    }
    if (queryRtPipe.rayTraversalPrimitiveCulling) {
        enableRtPipe.rayTraversalPrimitiveCulling = VK_TRUE;
    }
    if (queryRtPipe.rayTracingPipelineShaderGroupHandleCaptureReplay) {
        enableRtPipe.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_TRUE;
    }
    if (enableRayQuery && queryRayQuery.rayQuery) {
        enableRayQueryFeat.rayQuery = VK_TRUE;
    }
    if (queryAccel.accelerationStructureCaptureReplay) {
        enableAccel.accelerationStructureCaptureReplay = VK_TRUE;
    }
    if (queryAccel.accelerationStructureIndirectBuild) {
        enableAccel.accelerationStructureIndirectBuild = VK_TRUE;
    }
    if (queryAccel.descriptorBindingAccelerationStructureUpdateAfterBind) {
        enableAccel.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
    }
    deviceCi.pNext = &enableFeatures2;
    deviceCi.pEnabledFeatures = nullptr;
    if (vkCreateDevice(m_physicalDevice, &deviceCi, nullptr, &m_device) != VK_SUCCESS) {
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
UniquePtr<RHIPipeline> RHIVkContext::createGraphicsPipeline(
    const RHIGraphicsPipelineDescriptor& desc) {
    return createRhiVkGraphicsPipeline(desc);
}

UniquePtr<RHIDescriptorBuffer> RHIVkContext::createDescriptorBuffer(const RHIDescriptorBufferCreateInfo& createInfo)
{
    return createRhiVkDescriptorBuffer(createInfo);
}

void RHIVkContext::deviceWaitIdle() {
    vkDeviceWaitIdle(m_device);
}

const RHIDescriptorWriterOps* RHIVkContext::getDescriptorWriterOps() const {
    return getVkDescriptorWriterOps();
}

UniquePtr<RHIDescriptorSetLayout> RHIVkContext::createDescriptorSetLayout(InitializerList<RHIDescriptorSetBindingDesc> bindings) {
    return RHIVkDescriptorSetLayout::createUnique(this, bindings);
}

UniquePtr<RHIPipelineLayout> RHIVkContext::createPipelineLayout(const RHIPipelineLayoutCreateInfo& info) {
    return RHIVkPipelineLayout::createUnique(this, info);
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
UniquePtr<RHIVkPipeline> RHIVkContext::createRhiVkGraphicsPipeline(
    const RHIGraphicsPipelineDescriptor& desc) {
    return RHIVkPipeline::createUniqueGraphics(this, desc);
}

UniquePtr<RHIVkDescriptorBuffer> RHIVkContext::createRhiVkDescriptorBuffer(const RHIDescriptorBufferCreateInfo& createInfo)
{
    return RHIVkDescriptorBuffer::createUnique(this, createInfo);
}

} // namespace RHI::Vulkan
