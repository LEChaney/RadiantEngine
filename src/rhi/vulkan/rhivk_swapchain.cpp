#include "rhi/vulkan/rhivk_swapchain.h"
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_image_view.h"
#include "rhi/rhi_command_buffer.h"
#include "rhi/rhi_swapchain.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cassert>
#include <stdexcept>

#include <SDL.h>
#include <SDL_vulkan.h>

namespace rhi {
namespace vulkan {

RHIVKSwapchain::RHIVKSwapchain(RHIVKContext* context, SDL_Window* window, uint32_t width, uint32_t height, uint32_t buffer_count)
    : m_context(context), m_image_count(buffer_count) {
    // Create swapchain surface using SDL
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface(window, m_context->get_vk_instance(), &surface) != SDL_TRUE) {
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    // Create swapchain (minimal, not handling oldSwapchain, formats, etc.)
    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = buffer_count;
    swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM; // Hardcoded for test
    swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainInfo.imageExtent = { width, height };
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
    VkDevice device = context->get_vk_device();
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkResult res = vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain);
    assert(res == VK_SUCCESS);

    // Get swapchain images
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

    // Create image views and command buffers
    m_image_count = imageCount;
    m_image_views.resize(imageCount);
    m_command_buffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        // Create image view for each swapchain image
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainInfo.imageFormat;
        viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        VkImageView imageView;
        VkResult viewRes = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
        assert(viewRes == VK_SUCCESS && "Failed to create image view for swapchain image");
        m_image_views[i] = new RHIVKImageView(imageView, device);
        m_command_buffers[i] = static_cast<RHIVKCommandBuffer*>(context->create_command_buffer());
    }
    m_swapchain = swapchain;
    m_frame_index = 0;
}

RHIVKSwapchain::~RHIVKSwapchain() {
    for (auto* cb : m_command_buffers) delete cb;
    for (auto* iv : m_image_views) delete iv;
    VkDevice device = m_context->get_vk_device();
    if (m_swapchain) vkDestroySwapchainKHR(device, m_swapchain, nullptr);
    if (m_surface) vkDestroySurfaceKHR(m_context->get_vk_instance(), m_surface, nullptr);
}

RHISwapchain::RHIFrame RHIVKSwapchain::acquire_next_frame() {
    // Minimal: just cycle through images
    uint32_t idx = m_frame_index % m_image_count;
    m_frame_index++;
    return RHIFrame{ idx, m_image_views[idx], m_command_buffers[idx] };
}

void RHIVKSwapchain::present(const RHIFrame& frame) {
    // Minimal: no actual present, just stub for test
}

uint32_t RHIVKSwapchain::image_count() const {
    return m_image_count;
}

void RHIVKSwapchain::resize(uint32_t width, uint32_t height) {
    // TODO: Recreate swapchain with new size
}

} // namespace vulkan
} // namespace rhi
