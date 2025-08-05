#include "rhi/vulkan/swapchain/RHIVkSwapchain.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/command/RHIVkCommandBuffer.h"
#include "rhi/vulkan/queue/RHIVkQueue.h"
#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/sync/RHIVkSemaphore.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cassert>
#include <stdexcept>

#include <SDL.h>
#include <SDL_vulkan.h>

namespace rhi::vulkan {

UniquePtr<RHIVkSwapchain> RHIVkSwapchain::createUnique(
    RHIVkContext* context,
    SDL_Window* window,
    uint32_t width,
    uint32_t height,
    uint32_t imageCount)
{
    return UniquePtr<RHIVkSwapchain>(new RHIVkSwapchain(context, window, width, height, imageCount));
}

RHIVkSwapchain::RHIVkSwapchain(
    RHIVkContext* context,
    SDL_Window* window,
    uint32_t width,
    uint32_t height,
    uint32_t imageCount)
    : m_rhiContext(context)
    , m_imageCount(imageCount)
{
    // Create swapchain surface using SDL
    if (SDL_Vulkan_CreateSurface(window, m_rhiContext->getVkInstance(), &m_surface) != SDL_TRUE) {
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    // Query supported surface formats
    VkPhysicalDevice physicalDevice = m_rhiContext->getVkPhysicalDevice();
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);
    ASSERT(formatCount > 0 && "No surface formats available");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, formats.data());

    // Preferred formats in order (Hardcoded for now)
    // TODO: Support HDR formats and more flexible format selection
    const StaticArray<VkSurfaceFormatKHR, 2> preferredFormats = {
        VkSurfaceFormatKHR{ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
    };

    m_surfaceFormat = formats[0];
    bool found = false;
    for (const auto& pref : preferredFormats) {
        for (const auto& fmt : formats) {
            if (fmt.format == pref.format && fmt.colorSpace == pref.colorSpace) {
                m_surfaceFormat = fmt;
                found = true;
                break;
            }
        }
        if (found) {
            break;
        }
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = m_surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = m_surfaceFormat.format;
    swapchainInfo.imageColorSpace = m_surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = { width, height };
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
    VkDevice vkDevice = context->getVkDevice();
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(vkDevice, &swapchainInfo, nullptr, &swapchain);
    ASSERT(result == VK_SUCCESS);

    // Get swapchain images
    vkGetSwapchainImagesKHR(vkDevice, swapchain, &imageCount, nullptr);
    Array<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(vkDevice, swapchain, &imageCount, images.data());

    // Create image views and command buffers
    m_imageCount = imageCount;
    m_rhiImages.resize(imageCount);
    m_rhiImageViews.resize(imageCount);
    m_rhiCommandBuffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        m_rhiImages[i] = RHIVkImage::createUnique(
            m_rhiContext,
            images[i],
            width,
            height,
            toRhiFormat(m_surfaceFormat.format),
            false // swapchain owns the image
        );

        m_rhiImageViews[i] = RHIVkImageView::createUnique(
            m_rhiContext,
            m_rhiImages[i].get()
        );
        m_rhiCommandBuffers[i] = context->createRhiVkCommandBuffer();
    }
    m_swapchain = swapchain;
    m_imageIndex = 0;

    // Initialize semaphores for image acquisition
    m_usedRhiAcquireSemaphores.resize(m_imageCount);
    m_freeRhiAcquireSemaphores.resize(m_imageCount);
    for (uint32_t i = 0; i < m_imageCount; ++i) {
        m_usedRhiAcquireSemaphores[i] = RHIVkSemaphore::createUnique(m_rhiContext);
        m_freeRhiAcquireSemaphores[i] = RHIVkSemaphore::createUnique(m_rhiContext);
    }

    // Initialize fences for synchronization
    m_rhiFences.resize(m_imageCount);
    for (uint32_t i = 0; i < m_imageCount; ++i) {
        m_rhiFences[i] = RHIVkFence::createUnique(m_rhiContext);
    }
}

RHIVkSwapchain::~RHIVkSwapchain() {
    VkDevice vkDevice = m_rhiContext->getVkDevice();

    // Wait for present operations to complete
    vkQueueWaitIdle(m_rhiContext->getVkGraphicsQueue()->getVk());

    if (m_swapchain) {
        vkDestroySwapchainKHR(vkDevice, m_swapchain, nullptr);
    }
    if (m_surface) {
        vkDestroySurfaceKHR(m_rhiContext->getVkInstance(), m_surface, nullptr);
    }
}


RHISwapchain::RHIFrame RHIVkSwapchain::acquireNextFrame() {
    VkDevice device = m_rhiContext->getVkDevice();

    // This is a guess for the next image index and may change based on actual acquire result
    m_imageIndex = (m_imageIndex + 1) % m_imageCount;

    // Acquire the next image from the swapchain using one of the free semaphores
    auto& usedAcquireSemaphore = m_freeRhiAcquireSemaphores[m_imageIndex];
    VkResult acquireResult = vkAcquireNextImageKHR(
        device,
        m_swapchain,
        UINT64_MAX, // timeout
        usedAcquireSemaphore->getVk(),
        VK_NULL_HANDLE,
        &m_imageIndex
    );
    ASSERT(acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR);

    // Swap used acquire semaphore with the one for the current image index to track semaphore usage.
    std::swap(
        usedAcquireSemaphore,
        m_usedRhiAcquireSemaphores[m_imageIndex]
    );

    return RHIFrame{
        m_imageIndex,
        m_rhiImages[m_imageIndex].get(),
        m_rhiImageViews[m_imageIndex].get(),
        m_rhiCommandBuffers[m_imageIndex].get(),
        m_rhiFences[m_imageIndex].get()
    };
}

void RHIVkSwapchain::present(const RHIFrame& frame) {
    // Submit the present to the queue
    VkQueue queue = m_rhiContext->getVkGraphicsQueue()->getVk();
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &(m_usedRhiAcquireSemaphores[m_imageIndex]->getVk());
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &frame.imageIndex;
    presentInfo.pResults = nullptr;
    VkResult presentResult = vkQueuePresentKHR(queue, &presentInfo);
    ASSERT(presentResult == VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR);
}

uint32_t RHIVkSwapchain::imageCount() const {
    return m_imageCount;
}

void RHIVkSwapchain::resize(uint32_t width, uint32_t height) {
    // TODO: Recreate swapchain with new size
}

RHIFormat RHIVkSwapchain::getFormat() const
{
    return toRhiFormat(m_surfaceFormat.format);
}

RHIColorSpace RHIVkSwapchain::getColorSpace() const
{
    return toRhiColorSpace(m_surfaceFormat.colorSpace);
}

RHISurfaceFormat RHIVkSwapchain::getSurfaceFormat() const
{
    return toRhiSurfaceFormat(m_surfaceFormat);
}

} // namespace rhi::vulkan
