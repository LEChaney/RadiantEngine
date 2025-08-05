
#pragma once
#include "rhi/interface/command/RHICommandBuffer.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "core/CoreDefs.h"
#include "glm/vec4.hpp"
#include <vulkan/vulkan.h>

namespace rhi {

class RHIImage;
class RHIBuffer;

namespace vulkan {

class RHIVkContext;

class RHIVkCommandBuffer : public RHICommandBuffer {
public:
    static UniquePtr<RHIVkCommandBuffer> createUnique(RHIVkContext* context);
    ~RHIVkCommandBuffer() override;

    RHIVkCommandBuffer(const RHIVkCommandBuffer&) = delete;
    RHIVkCommandBuffer& operator=(const RHIVkCommandBuffer&) = delete;
    RHIVkCommandBuffer(RHIVkCommandBuffer&&) = delete;
    RHIVkCommandBuffer& operator=(RHIVkCommandBuffer&&) = delete;

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

private:
    RHIVkCommandBuffer(RHIVkContext* context);

    VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
    RHIVkContext* m_context = nullptr;
    Map<rhi::RHIImage*, RHIImageLayout> m_trackedImageLayouts;
};

} // namespace rhi::vulkan
} // namespace rhi