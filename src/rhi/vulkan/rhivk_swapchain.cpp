#include "rhi/vulkan/rhivk_swapchain.h"
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_image_view.h"
#include "rhi/vulkan/rhivk_image.h"
#include "rhi/rhi_command_buffer.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cassert>
#include <stdexcept>

#include <SDL.h>
#include <SDL_vulkan.h>

namespace rhi {
namespace vulkan {


RHIVKSwapchain::RHIVKSwapchain(RHIVKContext* context, SDL_Window* window, uint32_t width, uint32_t height, uint32_t image_count)
    : m_rhi_context(context)
    , m_surface(VK_NULL_HANDLE)
    , m_image_count(image_count) 
{
    // Create swapchain surface using SDL
    if (SDL_Vulkan_CreateSurface(window, m_rhi_context->get_vk_instance(), &m_surface) != SDL_TRUE) {
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    // Create swapchain (minimal, not handling oldSwapchain, formats, etc.)
    VkSwapchainCreateInfoKHR swapchain_info{};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = m_surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM; // Hardcoded for test
    swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchain_info.imageExtent = { width, height };
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped = VK_TRUE;
    swapchain_info.oldSwapchain = VK_NULL_HANDLE;
    VkDevice vk_device = context->get_vk_device();
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(vk_device, &swapchain_info, nullptr, &swapchain);
    ASSERT(result == VK_SUCCESS);

    // Get swapchain images
    vkGetSwapchainImagesKHR(vk_device, swapchain, &image_count, nullptr);
    Array<VkImage> images(image_count);
    vkGetSwapchainImagesKHR(vk_device, swapchain, &image_count, images.data());

    // Create image views and command buffers
    m_image_count = image_count;
    m_rhi_images.resize(image_count);
    m_rhi_image_views.resize(image_count);
    m_rhi_command_buffers.resize(image_count);
    for (uint32_t i = 0; i < image_count; ++i) {
        m_rhi_images[i] = make_unique<RHIVKImage>(images[i], vk_device, false); // swapchain owns the image
        
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = swapchain_info.imageFormat;
        view_info.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                                VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        VkImageView image_view;
        VkResult view_result = vkCreateImageView(vk_device, &view_info, nullptr, &image_view);
        ASSERT(view_result == VK_SUCCESS && "Failed to create image view for swapchain image");
        
        m_rhi_image_views[i] = make_unique<RHIVKImageView>(image_view, m_rhi_images[i].get(), m_rhi_context);
        m_rhi_command_buffers[i] = context->create_vk_command_buffer();
    }
    m_swapchain = swapchain;
    m_frame_index = 0;
}

RHIVKSwapchain::~RHIVKSwapchain() {
    VkDevice vk_device = m_rhi_context->get_vk_device();
    if (m_swapchain) {
        vkDestroySwapchainKHR(vk_device, m_swapchain, nullptr);
    }
    if (m_surface) {
        vkDestroySurfaceKHR(m_rhi_context->get_vk_instance(), m_surface, nullptr);
    }
}

RHISwapchain::RHIFrame RHIVKSwapchain::acquire_next_frame() {
    // Minimal: just cycle through images
    uint32_t image_index = m_frame_index % m_image_count;
    m_frame_index++;
    return RHIFrame{ 
        image_index, 
        m_rhi_images[image_index].get(), 
        m_rhi_image_views[image_index].get(), 
        m_rhi_command_buffers[image_index].get() 
    };
}

void RHIVKSwapchain::present(const RHIFrame& /*frame*/) {
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
