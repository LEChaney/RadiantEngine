#pragma once
#include "rhi/interface/descriptor/RHIDescriptorWriter.h"

namespace rhi::detail {

// Internal-only accessor for backend implementations.
// Not part of the public API surface.
struct RHIDescriptorWriterAccess {
    static RHIContext* ctx(const RHIDescriptorWriter& w) { return w.m_ctx; }
    static const RHIDescriptorSetData& data(const RHIDescriptorWriter& w) { return w.m_data; }
    static void* mapped(const RHIDescriptorWriter& w) { return w.m_data.getMapped(); }
};

} // namespace rhi::detail