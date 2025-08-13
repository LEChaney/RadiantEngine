#pragma once

// Ensure Win32 platform macros are available to Vulkan headers
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif

// Some SDK versions require this to expose provisional extension types like VK_EXT_descriptor_buffer
#ifndef VK_ENABLE_BETA_EXTENSIONS
#define VK_ENABLE_BETA_EXTENSIONS 1
#endif

#include <volk.h>

// Include beta header as a fallback for older SDKs (guarded inside the header itself)
#include <vulkan/vulkan_beta.h>
