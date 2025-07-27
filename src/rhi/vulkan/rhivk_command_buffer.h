
#pragma once
#include "rhi/rhi_command_buffer.h"
#include "glm/vec4.hpp"
#include <vulkan/vulkan.h>

class rhi::RHIImage;
class rhi::RHIBuffer;

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKCommandBuffer : public RHICommandBuffer {
public:
    RHIVKCommandBuffer(VkCommandBuffer cmd_buffer, RHIVKContext* context);
    ~RHIVKCommandBuffer() override;
    
    VkCommandBuffer get_vk() const { return m_cmd_buffer; }
    RHIVKCommandBuffer(const RHIVKCommandBuffer&) = delete;
    RHIVKCommandBuffer& operator=(const RHIVKCommandBuffer&) = delete;
    RHIVKCommandBuffer(RHIVKCommandBuffer&&) = delete;
    RHIVKCommandBuffer& operator=(RHIVKCommandBuffer&&) = delete;

    void begin() override;
    void end() override;
    void reset() override;
    void clear_color(rhi::RHIImage* image, const glm::vec4& color) override;
    // Transition image layout using internal layout tracking for old layout
    void transition_image_layout(class RHIImage* image, ImageLayout new_layout) override;
    // Transition image layout with explicit old layout
    void transition_image_layout(class RHIImage* image, ImageLayout old_layout, ImageLayout new_layout) override;
    void copy_image_to_buffer(rhi::RHIImage* image, rhi::RHIBuffer* buffer, uint32_t width, uint32_t height) override;

private:
    VkCommandBuffer m_cmd_buffer;
    RHIVKContext* m_context;
    Map<rhi::RHIImage*, ImageLayout> m_tracked_image_layouts;
};

} // namespace rhi::vulkan
