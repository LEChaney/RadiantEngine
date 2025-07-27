#pragma once
#include "rhi/rhi_core_defs.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi {
namespace vulkan {

// RHIFormat <-> VkFormat
VkFormat to_vk_format(RHIFormat fmt);
RHIFormat to_rhi_format(VkFormat fmt);

// RHIColorSpace <-> VkColorSpaceKHR
VkColorSpaceKHR to_vk_color_space(RHIColorSpace cs);
RHIColorSpace to_rhi_color_space(VkColorSpaceKHR cs);

// RHISurfaceFormat <-> VkSurfaceFormatKHR
VkSurfaceFormatKHR to_vk_surface_format(const RHISurfaceFormat& fmt);
RHISurfaceFormat to_rhi_surface_format(const VkSurfaceFormatKHR& vkfmt);

} // namespace vulkan
} // namespace rhi