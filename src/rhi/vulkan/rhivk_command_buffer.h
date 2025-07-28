
#pragma once
#include "rhi/rhi_command_buffer.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include "core/core_defs.h"
#include "glm/vec4.hpp"
#include <vulkan/vulkan.h>

namespace rhi {

class RHIImage;
class RHIBuffer;

namespace vulkan {

class RHIVKContext;

class RHIVKCommandBuffer : public RHICommandBuffer {
public:
    static UniquePtr<RHIVKCommandBuffer> createUnique(RHIVKContext* context);
    ~RHIVKCommandBuffer() override;

    void begin() override;
    void end() override;
    void reset() override;
    void clearColor(RHIImage* image, const glm::vec4& color) override;
    // Transition image layout using internal layout tracking for old layout
    void transitionImageLayout(RHIImage* image, RHIImageLayout newLayout) override;
    // Transition image layout with explicit old layout
    void transitionImageLayout(RHIImage* image, RHIImageLayout oldLayout, RHIImageLayout newLayout) override;
    void copyImageToBuffer(RHIImage* image, RHIBuffer* buffer, uint32_t width, uint32_t height) override;

    VkCommandBuffer getVk() const { return m_cmdBuffer; }

protected:
    RHIVKCommandBuffer(RHIVKContext* context);
    RHIVKCommandBuffer(const RHIVKCommandBuffer&) = delete;
    RHIVKCommandBuffer& operator=(const RHIVKCommandBuffer&) = delete;
    RHIVKCommandBuffer(RHIVKCommandBuffer&&) = delete;
    RHIVKCommandBuffer& operator=(RHIVKCommandBuffer&&) = delete;
    
private:
    VkCommandBuffer m_cmdBuffer;
    RHIVKContext* m_context;
    Map<rhi::RHIImage*, RHIImageLayout> m_trackedImageLayouts;
};

} // namespace rhi::vulkan
} // namespace rhi