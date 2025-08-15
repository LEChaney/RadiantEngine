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

    // Enable required base device extensions
    Array<const char*> deviceExtensions = { 
        "VK_KHR_swapchain",
        "VK_KHR_buffer_device_address",
        "VK_EXT_descriptor_buffer",
        "VK_EXT_descriptor_indexing",
        "VK_KHR_synchronization2"
    };
    // Always attempt to enable ray tracing pipeline (callable shaders, etc.) if supported.
    // Ray query remains optional and is only enabled if present.
    bool enableRayTracingPipeline = false;
    bool enableRayQuery = false;
    {
        uint32 devExtCount = 0;
        vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &devExtCount, nullptr);
        Array<VkExtensionProperties> devExts(devExtCount);
        vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &devExtCount, devExts.data());
        auto hasDevExt = [&devExts](const char* name) {
            return std::ranges::any_of(devExts, [name](const VkExtensionProperties& e){ return strcmp(e.extensionName, name) == 0; });
        };
        const bool hasDeferredHost = hasDevExt("VK_KHR_deferred_host_operations");
        const bool hasAccel        = hasDevExt("VK_KHR_acceleration_structure");
        const bool hasRtPipeline   = hasDevExt("VK_KHR_ray_tracing_pipeline");
        const bool hasRayQueryExt  = hasDevExt("VK_KHR_ray_query");

        if (hasDeferredHost && hasAccel && hasRtPipeline) {
            deviceExtensions.push_back("VK_KHR_deferred_host_operations");
            deviceExtensions.push_back("VK_KHR_acceleration_structure");
            deviceExtensions.push_back("VK_KHR_ray_tracing_pipeline");
            enableRayTracingPipeline = true;
        }
        if (hasRayQueryExt && hasAccel) { // ray query also needs acceleration structure
            deviceExtensions.push_back("VK_KHR_ray_query");
            enableRayQuery = true;
        }
    }
    // Enforce requirement: ray tracing pipeline must be available for engine (future callable shaders / ReSTIR)
    if (!enableRayTracingPipeline) {
        throw std::runtime_error("Required ray tracing pipeline extensions not supported by selected GPU");
    }
    createInfo.enabledExtensionCount = static_cast<uint32>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // --- Feature enabling --------------------------------------------------
    // We use VkPhysicalDeviceFeatures2 + promoted Vulkan 1.2/1.3 feature structs
    // so that we can explicitly enable the features GPU-Assisted Validation
    // would otherwise auto-enable (emitting WARNINGs about "Forcing ... to VK_TRUE").
    // By querying first, we only enable what the device supports, suppressing those warnings.

    // Query chain
    VkPhysicalDeviceFeatures2 queryFeatures2{}; queryFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    VkPhysicalDeviceDescriptorBufferFeaturesEXT queryDescBuffer{}; queryDescBuffer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    VkPhysicalDeviceVulkan13Features queryVulkan13{}; queryVulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceVulkan12Features queryVulkan12{}; queryVulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR queryAccel{}; queryAccel.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR queryRayTracingPipeline{}; queryRayTracingPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    VkPhysicalDeviceRayQueryFeaturesKHR queryRayQuery{}; queryRayQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

    queryFeatures2.pNext = &queryDescBuffer;
    queryDescBuffer.pNext = &queryVulkan13;
    queryVulkan13.pNext = &queryVulkan12;
    if (enableRayTracingPipeline || enableRayQuery) {
        queryVulkan12.pNext = &queryAccel;
        if (enableRayTracingPipeline) {
            queryAccel.pNext = &queryRayTracingPipeline;
            if (enableRayQuery) {
                queryRayTracingPipeline.pNext = &queryRayQuery;
            }
        } else if (enableRayQuery) { // only ray query (rare without RT pipeline)
            queryAccel.pNext = &queryRayQuery;
        }
    }

    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &queryFeatures2);

    // Enable chain (separate structs so we don't accidentally modify the query copy)
    VkPhysicalDeviceFeatures2 enableFeatures2{}; enableFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    VkPhysicalDeviceDescriptorBufferFeaturesEXT enableDescBuffer{}; enableDescBuffer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    VkPhysicalDeviceVulkan13Features enableVulkan13{}; enableVulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    VkPhysicalDeviceVulkan12Features enableVulkan12{}; enableVulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR enableAccel{}; enableAccel.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR enableRayTracingPipelineFeatures{}; enableRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    VkPhysicalDeviceRayQueryFeaturesKHR enableRayQueryFeatures{}; enableRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

    enableFeatures2.pNext = &enableDescBuffer;
    enableDescBuffer.pNext = &enableVulkan13;
    enableVulkan13.pNext = &enableVulkan12;
    if (enableRayTracingPipeline || enableRayQuery) {
        enableVulkan12.pNext = &enableAccel;
        if (enableRayTracingPipeline) {
            enableAccel.pNext = &enableRayTracingPipelineFeatures;
            if (enableRayQuery) {
                enableRayTracingPipelineFeatures.pNext = &enableRayQueryFeatures;
            }
        } else if (enableRayQuery) {
            enableAccel.pNext = &enableRayQueryFeatures;
        }
    }

    // Core (VkPhysicalDeviceFeatures) features required by GPU-AV instrumentation
    if (queryFeatures2.features.fragmentStoresAndAtomics) {
        enableFeatures2.features.fragmentStoresAndAtomics = VK_TRUE;
    }
    if (queryFeatures2.features.vertexPipelineStoresAndAtomics) {
        enableFeatures2.features.vertexPipelineStoresAndAtomics = VK_TRUE;
    }
    if (queryFeatures2.features.shaderInt64) {
        enableFeatures2.features.shaderInt64 = VK_TRUE;
    }

    // Vulkan 1.2 features
    if (queryVulkan12.descriptorIndexing) {
        enableVulkan12.descriptorIndexing = VK_TRUE;
    }
    if (queryVulkan12.bufferDeviceAddress) {
        enableVulkan12.bufferDeviceAddress = VK_TRUE;
    }
    if (queryVulkan12.bufferDeviceAddressCaptureReplay) {
        enableVulkan12.bufferDeviceAddressCaptureReplay = VK_TRUE;
    }
    if (queryVulkan12.storageBuffer8BitAccess) {
        enableVulkan12.storageBuffer8BitAccess = VK_TRUE;
    }
    if (queryVulkan12.uniformAndStorageBuffer8BitAccess) {
        enableVulkan12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    }
    if (queryVulkan12.storagePushConstant8) {
        enableVulkan12.storagePushConstant8 = VK_TRUE;
    }
    if (queryVulkan12.timelineSemaphore) {
        enableVulkan12.timelineSemaphore = VK_TRUE;
    }
    if (queryVulkan12.vulkanMemoryModel) {
        enableVulkan12.vulkanMemoryModel = VK_TRUE;
    }
    if (queryVulkan12.vulkanMemoryModelDeviceScope) {
        enableVulkan12.vulkanMemoryModelDeviceScope = VK_TRUE;
    }

    // Vulkan 1.3 features
    if (queryVulkan13.synchronization2) {
        enableVulkan13.synchronization2 = VK_TRUE;
    }

    // Descriptor buffer extension features (not yet fully supported by GPU-AV instrumentation)
    if (queryDescBuffer.descriptorBuffer) {
        enableDescBuffer.descriptorBuffer = VK_TRUE;
    }
#ifdef _DEBUG
    if (queryDescBuffer.descriptorBufferCaptureReplay) {
        enableDescBuffer.descriptorBufferCaptureReplay = VK_TRUE;
    }
#endif

    // Ray tracing pipeline / ray query / acceleration structure
    if (enableRayTracingPipeline || enableRayQuery) {
        if (!queryAccel.accelerationStructure) {
            throw std::runtime_error("Acceleration structure feature not supported despite extensions");
        }
        enableAccel.accelerationStructure = VK_TRUE;
        if (enableRayTracingPipeline) {
            if (!queryRayTracingPipeline.rayTracingPipeline) {
                throw std::runtime_error("Ray tracing pipeline feature not supported despite extensions");
            }
            enableRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
            // Optional enhancements if available
            if (queryRayTracingPipeline.rayTracingPipelineTraceRaysIndirect) {
                enableRayTracingPipelineFeatures.rayTracingPipelineTraceRaysIndirect = VK_TRUE;
            }
            if (queryRayTracingPipeline.rayTraversalPrimitiveCulling) {
                enableRayTracingPipelineFeatures.rayTraversalPrimitiveCulling = VK_TRUE;
            }
            if (queryRayTracingPipeline.rayTracingPipelineShaderGroupHandleCaptureReplay) {
                // Useful for offline shader group handle capture (debug / tooling)
                enableRayTracingPipelineFeatures.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_TRUE;
            }
        }
        if (enableRayQuery) {
            if (queryRayQuery.rayQuery) {
                enableRayQueryFeatures.rayQuery = VK_TRUE;
            }
        }
        // Optional acceleration structure extras
        if (queryAccel.accelerationStructureCaptureReplay) {
            enableAccel.accelerationStructureCaptureReplay = VK_TRUE;
        }
        if (queryAccel.accelerationStructureIndirectBuild) {
            enableAccel.accelerationStructureIndirectBuild = VK_TRUE;
        }
        if (queryAccel.descriptorBindingAccelerationStructureUpdateAfterBind) {
            enableAccel.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
        }
    }

    createInfo.pNext = &enableFeatures2; // Supply extended feature chain
    createInfo.pEnabledFeatures = nullptr; // Must be null when using Features2 chain

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
