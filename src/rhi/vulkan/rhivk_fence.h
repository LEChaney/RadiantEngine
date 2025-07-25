
#pragma once
#include "rhi/rhi_fence.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKFence : public rhi::RHIFence {
public:
    RHIVKFence(VkFence fence, VkDevice device);
    ~RHIVKFence();
    VkFence get_vk() const { return m_fence; }
private:
    VkFence m_fence;
    VkDevice m_device;
};

} // namespace rhi::vulkan
