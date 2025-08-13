#pragma once
#include "RHIDescriptorSetLayout.h"
#include "core/CoreDefs.h"
#include "rhi/interface/core/RHICoreDefs.h"

namespace rhi {

class RHIContext;

// Generic binding description collected by the builder
struct RHIDescriptorSetBindingDesc {
    uint32 binding = 0;
    RHIDescriptorType type = RHIDescriptorType::Sampler;
    RHIShaderStageFlags stageFlags = 0;
};

// Backend-dispatched ops for building a descriptor set layout from collected bindings
struct RHIDescriptorSetLayoutBuilderOps {
    UniquePtr<RHIDescriptorSetLayout> (*build)(RHIContext* ctx, const Array<RHIDescriptorSetBindingDesc>& bindings, RHIShaderStageFlags layoutStageFlags) = nullptr;
};

// Header-only, value-type builder. Does not own backend resources.
class RHIDescriptorSetLayoutBuilder {
public:
    RHIDescriptorSetLayoutBuilder(RHIContext* ctx)
        : m_ctx(ctx), m_ops(ctx->getDescriptorSetLayoutBuilderOps()) {}

    // Bind/replace ops (usually from context)
    void bindOps(const RHIDescriptorSetLayoutBuilderOps* ops) { m_ops = ops; }

    // Collect bindings (backend-agnostic)
    RHIDescriptorSetLayoutBuilder& addBinding(uint32 binding, RHIDescriptorType type, RHIShaderStageFlags stageFlags = 0) {
        m_bindings.push_back(RHIDescriptorSetBindingDesc{binding, type, stageFlags});
        return *this;
    }

    // Build backend layout using ops
    UniquePtr<RHIDescriptorSetLayout> build(RHIShaderStageFlags layoutStageFlags = 0) const {
        ASSERT(m_ctx && "RHIDescriptorSetLayoutBuilder requires a context");
        ASSERT(m_ops && m_ops->build && "RHIDescriptorSetLayoutBuilder ops not bound");
        return m_ops->build(m_ctx, m_bindings, layoutStageFlags);
    }

    // Accessors for backend
    RHIContext* getContext() const { return m_ctx; }
    const Array<RHIDescriptorSetBindingDesc>& bindings() const { return m_bindings; }

private:
    RHIContext* m_ctx = nullptr;
    Array<RHIDescriptorSetBindingDesc> m_bindings;
    const RHIDescriptorSetLayoutBuilderOps* m_ops = nullptr;
};

} // namespace rhi