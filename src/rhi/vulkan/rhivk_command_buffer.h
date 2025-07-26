
#pragma once
#include "rhi/vulkan/rhivk_context.h"
#include "rhi/rhi_command_buffer.h"
#include "rhi/rhi_image.h"
#include "glm/vec4.hpp"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKCommandBuffer : public RHICommandBuffer {
public:
    RHIVKCommandBuffer(VkCommandBuffer cmd_buffer, RHIVKContext* context);
    ~RHIVKCommandBuffer() override;

    void begin() override;
    void end() override;
    void clear_color(rhi::RHIImage* image, const glm::vec4& color) override;

    VkCommandBuffer get_vk() const { return m_cmd_buffer; }
    RHIVKCommandBuffer(const RHIVKCommandBuffer&) = delete;
    RHIVKCommandBuffer& operator=(const RHIVKCommandBuffer&) = delete;
    RHIVKCommandBuffer(RHIVKCommandBuffer&&) = delete;
    RHIVKCommandBuffer& operator=(RHIVKCommandBuffer&&) = delete;
private:
    VkCommandBuffer m_cmd_buffer;
    RHIVKContext* m_context;
};

} // namespace rhi::vulkan
