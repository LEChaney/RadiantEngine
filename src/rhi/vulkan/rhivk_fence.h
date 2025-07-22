
#pragma once
#include "rhi/fence.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKFence : public rhi::RHIFence {
public:
    RHIVKFence(VkFence fence, VkDevice device);
    ~RHIVKFence();
    VkFence get_vk() const { return fence_; }
private:
    VkFence fence_;
    VkDevice device_;
};

} // namespace rhi::vulkan
