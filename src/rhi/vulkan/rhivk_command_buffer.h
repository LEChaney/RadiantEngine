
#pragma once
#include "rhi/command_buffer.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKCommandBuffer : public rhi::RHICommandBuffer {
public:
    RHIVKCommandBuffer(VkCommandBuffer cmdBuffer, VkDevice device, VkCommandPool pool);
    void begin() override;
    void end() override;
    VkCommandBuffer get_vk() const { return cmdBuffer_; }
private:
    VkCommandBuffer cmdBuffer_;
    VkDevice device_;
    VkCommandPool pool_;
};

} // namespace rhi::vulkan
