#pragma once
#include "rhi/rhi_image.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKImage : public rhi::RHIImage {
public:
    RHIVKImage(VkImage image, VkDevice device, bool owns_image = true);
    ~RHIVKImage() override;

    VkImage get_vk() const { return m_image; }
    RHIVKImage(const RHIVKImage&) = delete;
    RHIVKImage& operator=(const RHIVKImage&) = delete;
    RHIVKImage(RHIVKImage&&) = delete;
    RHIVKImage& operator=(RHIVKImage&&) = delete;
private:
    VkImage m_image;
    VkDevice m_device;
    bool m_owns_image; // If true, this class will manage the Vulkan image's lifetime
};

} // namespace rhi::vulkan
