#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"

namespace rhi {

class RHIShaderModule {
public:
    virtual ~RHIShaderModule() = default;

    RHIShaderModule(const RHIShaderModule&) = delete;
    RHIShaderModule& operator=(const RHIShaderModule&) = delete;
    RHIShaderModule(RHIShaderModule&&) = delete;
    RHIShaderModule& operator=(RHIShaderModule&&) = delete;

protected:
    RHIShaderModule() = default;
};

} // namespace rhi
