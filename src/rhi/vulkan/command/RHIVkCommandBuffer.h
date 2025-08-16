#pragma once
#include "rhi/interface/command/RHICommandBuffer.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "core/CoreDefs.h"
#include "glm/vec4.hpp"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace RHI {

class RHIImage;
class RHIBuffer;
class RHIPipeline;
class RHIPipelineLayout;
class RHIDescriptorSet;

namespace Vulkan {

class RHIVkContext;
class RHIVkPipeline;
class RHIVkPipelineLayout;
class RHIVkBuffer;

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
    void bindComputePipeline(RHIPipeline* pipeline) override;
    void bindGraphicsPipeline(RHIPipeline* pipeline) override;
    void bindDescriptorBuffers(const Array<RHIDescriptorBuffer*>& descBuffers) override;
    void bindDescriptorSets(const Array<RHIDescriptorSetBinding>& setBindings,
        RHIPipelineLayout* pipelineLayout, RHIPipelineBindPoint bindPoint) override;
    void pushConstants(RHIPipelineLayout* layout, RHIShaderStageFlags shaderStageFlags,
        uint32 offset, uint32 size, const void* data) override;
    void dispatch(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ) override;

    VkCommandBuffer getVk() const { return m_cmdBuffer; }

private:
    RHIVkCommandBuffer(RHIVkContext* context);

    void bindVkPipeline(VkPipelineBindPoint bindPoint, RHIPipeline* pipeline);

    VkCommandBuffer m_cmdBuffer = VK_NULL_HANDLE;
    RHIVkContext* m_context = nullptr;
    Map<RHIImage*, RHIImageLayout> m_trackedImageLayouts;
};

} // namespace rhi::vulkan
} // namespace rhi