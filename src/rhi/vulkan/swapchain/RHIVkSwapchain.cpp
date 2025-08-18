#include "rhi/vulkan/swapchain/RHIVkSwapchain.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/queue/RHIVkQueue.h"
#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/sync/RHIVkSemaphore.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"
#include <stdexcept>

#include <SDL.h>
#include <SDL_vulkan.h>

namespace RHI::Vulkan {

UniquePtr<RHIVkSwapchain> RHIVkSwapchain::createUnique(
    RHIVkContext* ctx,
    const RHISwapchainCreateInfo& info)
{
    return UniquePtr<RHIVkSwapchain>(new RHIVkSwapchain(ctx, info));
}

RHIVkSwapchain::RHIVkSwapchain(
    RHIVkContext* context,
    const RHISwapchainCreateInfo& info)
    : m_ctx(context)
    , m_imageCount(info.imageCount)
{
    // Create swapchain surface using SDL
    if (SDL_Vulkan_CreateSurface(info.window, m_ctx->getVkInstance(), &m_surface) != SDL_TRUE) {
        throw std::runtime_error("Failed to create Vulkan surface");
    }

    // Query supported surface formats
    VkPhysicalDevice physicalDevice = m_ctx->getVkPhysicalDevice();
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
    swapchainInfo.minImageCount = info.imageCount;
    swapchainInfo.imageFormat = m_surfaceFormat.format;
    swapchainInfo.imageColorSpace = m_surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = { info.width, info.height };
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageUsage |= toVkImageUsageFlags(info.extraColorUsage);
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
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(vkDevice, swapchain, &imageCount, nullptr);
    SmallArray<VkImage, 4> images(imageCount);
    vkGetSwapchainImagesKHR(vkDevice, swapchain, &imageCount, images.data());

    // Create image views and command buffers
    m_imageCount = imageCount;
    m_colorImgs.resize(imageCount);
    m_colorImgViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        m_colorImgs[i] = RHIVkImage::createUnique(
            m_ctx,
            images[i],
            info.width,
            info.height,
            toRhiFormat(m_surfaceFormat.format),
            toRhiImageUsageFlags(swapchainInfo.imageUsage),
            false // swapchain owns the image
        );
        m_colorImgViews[i] = RHIVkImageView::createUnique(
            m_ctx,
            m_colorImgs[i].get(),
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        m_depthImgs.push_back(RHIVkImage::createUnique(
            m_ctx,
            info.width,
            info.height,
            info.depthFormat,
            info.extraDepthUsage | RHIImageUsage::DepthStencilAttachment,
            RHIMemoryProperty::DeviceLocal
        ));
        m_depthImgViews.push_back(RHIVkImageView::createUnique(
            m_ctx,
            m_depthImgs[i].get(),
            info.depthFormat == RHIFormat::RHI_FORMAT_D32_SFLOAT ?
                VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
        ));
    }
    m_swapchain = swapchain;
    m_imageIndex = 0;
}

RHIVkSwapchain::~RHIVkSwapchain() {
    VkDevice vkDevice = m_ctx->getVkDevice();

    // Wait for present operations to complete
    vkQueueWaitIdle(m_ctx->getVkGraphicsQueue()->getVk());

    if (m_swapchain) {
        vkDestroySwapchainKHR(vkDevice, m_swapchain, nullptr);
    }
    if (m_surface) {
        vkDestroySurfaceKHR(m_ctx->getVkInstance(), m_surface, nullptr);
    }
}


uint32 RHIVkSwapchain::acquireNextImage(RHISemaphore* imageAvailableSemaphore) {
    VkDevice device = m_ctx->getVkDevice();
    VkSemaphore vkSemaphore = imageAvailableSemaphore ? static_cast<RHIVkSemaphore*>(imageAvailableSemaphore)->getVk() : VK_NULL_HANDLE;
    VkResult acquireResult = vkAcquireNextImageKHR(
        device,
        m_swapchain,
        UINT64_MAX,
        vkSemaphore,
        VK_NULL_HANDLE,
        &m_imageIndex
    );
    ASSERT(acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR);
    return m_imageIndex;
}

void RHIVkSwapchain::present(uint32 imageIndex, RHISemaphore* waitSemaphore) {
    // Submit the present to the queue
    VkQueue queue = m_ctx->getVkGraphicsQueue()->getVk();
    VkSemaphore vkWaitSemaphore = waitSemaphore ?
        static_cast<RHIVkSemaphore*>(waitSemaphore)->getVk() : VK_NULL_HANDLE;
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &vkWaitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;
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

RHIFormat RHIVkSwapchain::getColorFormat() const
{
    return toRhiFormat(m_surfaceFormat.format);
}

RHIFormat RHIVkSwapchain::getDepthFormat() const {
    return m_depthImgs[0]->getFormat();
}

RHIColorSpace RHIVkSwapchain::getColorSpace() const
{
    return toRhiColorSpace(m_surfaceFormat.colorSpace);
}

RHISurfaceFormat RHIVkSwapchain::getSurfaceFormat() const
{
    return toRhiSurfaceFormat(m_surfaceFormat);
}

RHIImage * RHIVkSwapchain::getColorImage(uint32 imageIndex) {
    return m_colorImgs[imageIndex].get();
}

RHIImageView * RHIVkSwapchain::getColorImageView(uint32 imageIndex) {
    return m_colorImgViews[imageIndex].get();
}

RHIImage* RHIVkSwapchain::getDepthImage(uint32 imageIndex) {
    return m_depthImgs[imageIndex].get();
}

RHIImageView* RHIVkSwapchain::getDepthImageView(uint32 imageIndex) {
    return m_depthImgViews[imageIndex].get();
}

} // namespace rhi::vulkan
