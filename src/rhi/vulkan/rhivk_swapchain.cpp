#include "rhi/vulkan/rhivk_swapchain.h"
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/vulkan/rhivk_command_buffer.h"
#include "rhi/vulkan/rhivk_queue.h"
#include "rhi/vulkan/rhivk_image_view.h"
#include "rhi/vulkan/rhivk_image.h"
#include "rhi/vulkan/rhivk_semaphore.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cassert>
#include <stdexcept>

#include <SDL.h>
#include <SDL_vulkan.h>
#include "rhivk_swapchain.h"

namespace rhi::vulkan {

UniquePtr<RHIVKSwapchain> RHIVKSwapchain::create_unique(
    RHIVKContext* context, 
    SDL_Window* window, 
    uint32_t width, 
    uint32_t height, 
    uint32_t image_count) 
{
    return UniquePtr<RHIVKSwapchain>(new RHIVKSwapchain(context, window, width, height, image_count));
}

RHIVKSwapchain::RHIVKSwapchain(
    RHIVKContext* context, 
    SDL_Window* window, 
    uint32_t width, 
    uint32_t height, 
    uint32_t image_count
)
    : m_rhi_context(context)
    , m_image_count(image_count) 
{
    // Create swapchain surface using SDL
    if (SDL_Vulkan_CreateSurface(window, m_rhi_context->get_vk_instance(), &m_surface) != SDL_TRUE) {
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    // Query supported surface formats
    VkPhysicalDevice physicalDevice = m_rhi_context->get_vk_physical_device();
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);
    ASSERT(formatCount > 0 && "No surface formats available");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, formats.data());

    // Preferred formats in order (Hardcoded for now)
    // TODO: Support HDR formats and more flexible format selection
    const VkSurfaceFormatKHR preferredFormats[] = {
        { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
    };

    m_surface_format = formats[0];
    bool found = false;
    for (const auto& pref : preferredFormats) {
        for (const auto& fmt : formats) {
            if (fmt.format == pref.format && fmt.colorSpace == pref.colorSpace) {
                m_surface_format = fmt;
                found = true;
                break;
            }
        }
        if (found) break;
    }

    VkSwapchainCreateInfoKHR swapchain_info{};
    swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface = m_surface;
    swapchain_info.minImageCount = image_count;
    swapchain_info.imageFormat = m_surface_format.format;
    swapchain_info.imageColorSpace = m_surface_format.colorSpace;
    swapchain_info.imageExtent = { width, height };
    swapchain_info.imageArrayLayers = 1;
    swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
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
        m_rhi_images[i] = RHIVKImage::create_unique(
            m_rhi_context,
            images[i],
            width,
            height,
            to_rhi_format(m_surface_format.format),
            false // swapchain owns the image
        );

        m_rhi_image_views[i] = RHIVKImageView::create_unique(
            m_rhi_context, 
            m_rhi_images[i].get()
        );
        m_rhi_command_buffers[i] = context->create_vk_command_buffer();
    }
    m_swapchain = swapchain;
    m_image_index = 0;

    // Initialize semaphores for image acquisition
    m_used_rhi_acquire_semaphores.resize(m_image_count);
    m_free_rhi_acquire_semaphores.resize(m_image_count);
    for (uint32_t i = 0; i < m_image_count; ++i) {
        m_used_rhi_acquire_semaphores[i] = RHIVKSemaphore::create_unique(m_rhi_context);
        m_free_rhi_acquire_semaphores[i] = RHIVKSemaphore::create_unique(m_rhi_context);
    }

    // Initialize fences for synchronization
    m_rhi_fences.resize(m_image_count);
    for (uint32_t i = 0; i < m_image_count; ++i) {
        m_rhi_fences[i] = RHIVKFence::create_unique(m_rhi_context);
    }
}

RHIVKSwapchain::~RHIVKSwapchain() {
    VkDevice vk_device = m_rhi_context->get_vk_device();

    // Wait for present operations to complete
    vkQueueWaitIdle(m_rhi_context->get_vk_graphics_queue()->get_vk());

    if (m_swapchain) {
        vkDestroySwapchainKHR(vk_device, m_swapchain, nullptr);
    }
    if (m_surface) {
        vkDestroySurfaceKHR(m_rhi_context->get_vk_instance(), m_surface, nullptr);
    }
}


RHISwapchain::RHIFrame RHIVKSwapchain::acquire_next_frame() {
    VkDevice device = m_rhi_context->get_vk_device();

    // This is a guess for the next image index and may change based on actual acquire result
    m_image_index = (m_image_index + 1) % m_image_count;

    // Acquire the next image from the swapchain using one of the free semaphores
    auto& used_acquire_semaphore = m_free_rhi_acquire_semaphores[m_image_index];
    VkResult acquire_result = vkAcquireNextImageKHR(
        device,
        m_swapchain,
        UINT64_MAX, // timeout
        used_acquire_semaphore->get_vk(),
        VK_NULL_HANDLE,
        &m_image_index
    );
    ASSERT(acquire_result == VK_SUCCESS || acquire_result == VK_SUBOPTIMAL_KHR);

    // Swap used acquire semaphore with the one for the current image index to track semaphore usage.
    std::swap(
        used_acquire_semaphore,
        m_used_rhi_acquire_semaphores[m_image_index]
    );

    return RHIFrame{
        m_image_index,
        m_rhi_images[m_image_index].get(),
        m_rhi_image_views[m_image_index].get(),
        m_rhi_command_buffers[m_image_index].get(),
        m_rhi_fences[m_image_index].get()
    };
}

void RHIVKSwapchain::present(const RHIFrame& frame) {
    // Submit the present to the queue
    VkQueue queue = m_rhi_context->get_vk_graphics_queue()->get_vk();
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &(m_used_rhi_acquire_semaphores[m_image_index]->get_vk());
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swapchain;
    present_info.pImageIndices = &frame.image_index;
    present_info.pResults = nullptr;
    VkResult present_result = vkQueuePresentKHR(queue, &present_info);
    ASSERT(present_result == VK_SUCCESS || present_result == VK_SUBOPTIMAL_KHR);
}

uint32_t RHIVKSwapchain::image_count() const {
    return m_image_count;
}

void RHIVKSwapchain::resize(uint32_t width, uint32_t height) {
    // TODO: Recreate swapchain with new size
}

RHIFormat RHIVKSwapchain::get_format() const
{
    return to_rhi_format(m_surface_format.format);
}

RHIColorSpace RHIVKSwapchain::get_color_space() const
{
    return to_rhi_color_space(m_surface_format.colorSpace);
}

RHISurfaceFormat RHIVKSwapchain::get_surface_format() const
{
    return to_rhi_surface_format(m_surface_format);
}

} // namespace rhi::vulkan
