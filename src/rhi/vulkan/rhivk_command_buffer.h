
#pragma once
#include "rhi/rhi_command_buffer.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include "core/core_defs.h"
#include "glm/vec4.hpp"
#include <vulkan/vulkan.h>

class rhi::RHIImage;
class rhi::RHIBuffer;

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKCommandBuffer : public RHICommandBuffer {
public:
    static UniquePtr<RHIVKCommandBuffer> create_unique(RHIVKContext* context);
    ~RHIVKCommandBuffer() override;

    void begin() override;
    void end() override;
    void reset() override;
    void clear_color(RHIImage* image, const glm::vec4& color) override;
    // Transition image layout using internal layout tracking for old layout
    void transition_image_layout(RHIImage* image, RHIImageLayout new_layout) override;
    // Transition image layout with explicit old layout
    void transition_image_layout(RHIImage* image, RHIImageLayout old_layout, RHIImageLayout new_layout) override;
    void copy_image_to_buffer(RHIImage* image, RHIBuffer* buffer, uint32_t width, uint32_t height) override;

    VkCommandBuffer get_vk() const { return m_cmd_buffer; }

protected:
    RHIVKCommandBuffer(RHIVKContext* context);
    RHIVKCommandBuffer(const RHIVKCommandBuffer&) = delete;
    RHIVKCommandBuffer& operator=(const RHIVKCommandBuffer&) = delete;
    RHIVKCommandBuffer(RHIVKCommandBuffer&&) = delete;
    RHIVKCommandBuffer& operator=(RHIVKCommandBuffer&&) = delete;
    
private:
    VkCommandBuffer m_cmd_buffer;
    RHIVKContext* m_context;
    Map<rhi::RHIImage*, RHIImageLayout> m_tracked_image_layouts;
};

} // namespace rhi::vulkan
