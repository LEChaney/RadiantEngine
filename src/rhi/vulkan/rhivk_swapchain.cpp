#include "rhi/vulkan/rhivk_swapchain.h"

namespace rhi {
namespace vulkan {

RHIVKSwapchain::RHIVKSwapchain(void* window, uint32_t width, uint32_t height, uint32_t buffer_count)
    : m_image_count(buffer_count) {
    // TODO: Initialize Vulkan swapchain, images, image views, etc.
}

RHIVKSwapchain::~RHIVKSwapchain() {
    // TODO: Cleanup Vulkan resources
}

Swapchain::Frame RHIVKSwapchain::acquire_next_frame() {
    // TODO: Acquire next image, return Frame struct
    return Frame{0, nullptr, nullptr};
}

void RHIVKSwapchain::present(const Frame& frame) {
    // TODO: Present image to swapchain
}

uint32_t RHIVKSwapchain::image_count() const {
    return m_image_count;
}

void RHIVKSwapchain::resize(uint32_t width, uint32_t height) {
    // TODO: Recreate swapchain with new size
}

} // namespace vulkan
} // namespace rhi
