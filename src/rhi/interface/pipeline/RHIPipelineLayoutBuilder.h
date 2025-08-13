#pragma once
#include "rhi/interface/core/RHIContext.h"
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"
#include "RHIPipelineLayout.h"

namespace rhi {

class RHIDescriptorSetLayout;

// Push constant range description collected by the builder
struct RHIPushConstantRangeDesc {
    RHIShaderStageFlags stages = 0;
    uint32 offset = 0;
    uint32 size = 0;
};

// Backend-dispatched ops for building a pipeline layout from collected inputs
struct RHIPipelineLayoutBuilderOps {
    UniquePtr<RHIPipelineLayout> (*build)(RHIContext* ctx,
                                          const Array<RHIDescriptorSetLayout*>& setLayouts,
                                          const Array<RHIPushConstantRangeDesc>& pushRanges) = nullptr;
};

// Header-only, value-type builder. Does not own backend resources.
class RHIPipelineLayoutBuilder {
public:
    explicit RHIPipelineLayoutBuilder(RHIContext* ctx)
        : m_ctx(ctx), m_ops(ctx->getPipelineLayoutBuilderOps()) {}

    // Collect descriptor set layouts
    RHIPipelineLayoutBuilder& addDescriptorSetLayout(RHIDescriptorSetLayout* layout) {
        m_setLayouts.push_back(layout);
        return *this;
    }

    // Collect push constant ranges
    RHIPipelineLayoutBuilder& addPushConstantRange(RHIShaderStageFlags stages, uint32 offset, uint32 size) {
        m_pushConstants.push_back(RHIPushConstantRangeDesc{stages, offset, size});
        return *this;
    }

    // Build pipeline layout using backend ops
    UniquePtr<RHIPipelineLayout> build() const {
        ASSERT(m_ctx && "RHIPipelineLayoutBuilder requires a context");
        ASSERT(m_ops && m_ops->build && "RHIPipelineLayoutBuilder ops not bound");
        return m_ops->build(m_ctx, m_setLayouts, m_pushConstants);
    }

    // Accessors for backend
    RHIContext* getContext() const { return m_ctx; }
    const Array<RHIDescriptorSetLayout*>& descriptorSetLayouts() const { return m_setLayouts; }
    const Array<RHIPushConstantRangeDesc>& pushConstantRanges() const { return m_pushConstants; }

private:
    RHIContext* m_ctx = nullptr;
    Array<RHIDescriptorSetLayout*> m_setLayouts;
    Array<RHIPushConstantRangeDesc> m_pushConstants;
    const RHIPipelineLayoutBuilderOps* m_ops = nullptr;
};

} // namespace rhi
