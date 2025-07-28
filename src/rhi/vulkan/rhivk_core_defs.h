#pragma once
#include "rhi/rhi_core_defs.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

// RHIFormat <-> VkFormat
VkFormat toVkFormat(RHIFormat fmt);
RHIFormat toRhiFormat(VkFormat fmt);

// RHIColorSpace <-> VkColorSpaceKHR
VkColorSpaceKHR toVkColorSpace(RHIColorSpace cs);
RHIColorSpace toRhiColorSpace(VkColorSpaceKHR cs);

// RHISurfaceFormat <-> VkSurfaceFormatKHR
VkSurfaceFormatKHR toVkSurfaceFormat(const RHISurfaceFormat& fmt);
RHISurfaceFormat toRhiSurfaceFormat(const VkSurfaceFormatKHR& vkfmt);

} // namespace rhi::vulkan