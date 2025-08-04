#pragma once
#include "rhi/rhi_core_defs.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

// TODO: Rename this file to rhivk_type_conversions.h or similar

namespace rhi::vulkan {

// RHIImageLayout <-> VkImageLayout
VkImageLayout toVkImageLayout(RHIImageLayout layout);
RHIImageLayout toRhiImageLayout(VkImageLayout layout);

// RHIDescriptorType <-> VkDescriptorType
VkDescriptorType toVkDescriptorType(RHIDescriptorType type);
RHIDescriptorType toRhiDescriptorType(VkDescriptorType type);

// RHIFormat <-> VkFormat
VkFormat toVkFormat(RHIFormat fmt);
RHIFormat toRhiFormat(VkFormat fmt);

// RHIColorSpace <-> VkColorSpaceKHR
VkColorSpaceKHR toVkColorSpace(RHIColorSpace cs);
RHIColorSpace toRhiColorSpace(VkColorSpaceKHR cs);

// RHISurfaceFormat <-> VkSurfaceFormatKHR
VkSurfaceFormatKHR toVkSurfaceFormat(const RHISurfaceFormat& fmt);
RHISurfaceFormat toRhiSurfaceFormat(const VkSurfaceFormatKHR& vkfmt);

// RHIBufferUsageFlags <-> VkBufferUsageFlags
VkBufferUsageFlags toVkBufferUsageFlags(RHIBufferUsageFlags flags);
RHIBufferUsageFlags toRhiBufferUsageFlags(VkBufferUsageFlags flags);

// RHIImageUsageFlags <-> VkImageUsageFlags
VkImageUsageFlags toVkImageUsageFlags(RHIImageUsageFlags flags);
RHIImageUsageFlags toRhiImageUsageFlags(VkImageUsageFlags flags);

// RHIMemoryPropertyFlags <-> VkMemoryPropertyFlags
VkMemoryPropertyFlags toVkMemoryPropertyFlags(RHIMemoryPropertyFlags flags);
RHIMemoryPropertyFlags toRhiMemoryPropertyFlags(VkMemoryPropertyFlags flags);

// RHIShaderStageFlags <-> VkShaderStageFlags
VkShaderStageFlags toVkShaderStageFlags(RHIShaderStageFlags flags);
RHIShaderStageFlags toRhiShaderStageFlags(VkShaderStageFlags flags);

} // namespace rhi::vulkan