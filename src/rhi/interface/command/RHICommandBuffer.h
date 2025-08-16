#pragma once
#include "rhi/interface/descriptor/RHIDescriptorBuffer.h"
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"
#include <glm/vec4.hpp>

namespace RHI {

class RHIPipeline;
class RHIPipelineLayout;
class RHIBuffer;
class RHIDescriptorSet;
class RHIImageView;
struct RHIDescriptorSetBinding;

// --- Simple dynamic rendering descriptors (minimal for now) ---
struct RHIRenderingAttachment {
    RHIImageView* view = nullptr;
    RHIImageLayout layout = RHIImageLayout::Undefined;
    // TODO: loadOp / storeOp
};

struct RHIRect2D {
    int32 x = 0;
    int32 y = 0;
    uint32 width = 0;
    uint32 height = 0;
};

struct RHIViewport {
    RHIRect2D viewRect;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

struct RHIRenderingInfo {
    SmallArray<RHIRenderingAttachment, 4> colorAttachments;
    RHIRenderingAttachment depthAttachment;
    RHIRenderingAttachment stencilAttachment;
    RHIRect2D renderArea;
};

class RHICommandBuffer {
public:
    virtual ~RHICommandBuffer() = default;

    RHICommandBuffer(const RHICommandBuffer&) = delete;
    RHICommandBuffer& operator=(const RHICommandBuffer&) = delete;
    RHICommandBuffer(RHICommandBuffer&&) = delete;
    RHICommandBuffer& operator=(RHICommandBuffer&&) = delete;
    
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void reset() = 0;
    virtual void clearColor(class RHIImage* image, const glm::vec4& color) = 0;
    // Transition image layout using internal layout tracking for old layout
    virtual void transitionImageLayout(class RHIImage* image, RHIImageLayout newLayout) = 0;
    // Transition image layout with explicit old layout
    virtual void transitionImageLayout(class RHIImage* image, RHIImageLayout oldLayout, RHIImageLayout newLayout) = 0;
    virtual void copyImageToBuffer(class RHIImage* image, class RHIBuffer* buffer, uint32 width, uint32 height) = 0;
    virtual void bindComputePipeline(RHIPipeline* pipeline) = 0;
    virtual void bindGraphicsPipeline(RHIPipeline* get) = 0;
    virtual void bindDescriptorBuffers(const Array<RHIDescriptorBuffer*>& descBuffers) = 0;
    virtual void bindDescriptorSets(const Array<RHIDescriptorSetBinding>& setBindings,
        RHIPipelineLayout* pipelineLayout, RHIPipelineBindPoint bindPoint) = 0;
    virtual void pushConstants(RHIPipelineLayout* layout, RHIShaderStageFlags shaderStageFlags,
        uint32 offset, uint32 size, const void* data) = 0;
    virtual void dispatchCompute(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ) = 0;
    virtual void dispatchMesh(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ) = 0;
    // Dynamic rendering begin/end
    virtual void beginDynRendering(const RHIRenderingInfo& info) = 0;
    virtual void endDynRendering() = 0;
    // Dynamic state helpers
    virtual void setViewport(const RHIViewport& viewport) = 0;
    virtual void setScissor(const RHIRect2D& scissorRect) = 0;

    RHIPipeline* getBoundPipeline() const {
        return m_boundPipeline;
    }
    uint32 getBoundDescriptorBufferCount() const {
        return m_boundDescriptorBuffers.size();
    }
    RHIDescriptorBuffer* getBoundDescriptorBuffer(uint32 index) const {
        return m_boundDescriptorBuffers[index];
    }
    uint32 getBoundDescriptorBufferIndex(RHIDescriptorBuffer* buffer) const {
        return m_boundDescriptorBuffersToIndex.at(buffer);
    }

protected:
    // Only derived context or implementation should create RHICommandBuffer objects
    RHICommandBuffer() = default;

    RHIPipeline* m_boundPipeline = nullptr;
    Array<RHIDescriptorBuffer*> m_boundDescriptorBuffers;
    Map<RHIDescriptorBuffer*, uint32> m_boundDescriptorBuffersToIndex;
};

} // namespace rhi
