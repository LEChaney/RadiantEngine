#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"

namespace rhi {

class RHIPipelineLayout {
public:
    virtual ~RHIPipelineLayout() = default;

    RHIPipelineLayout(const RHIPipelineLayout&) = delete;
    RHIPipelineLayout& operator=(const RHIPipelineLayout&) = delete;
    RHIPipelineLayout(RHIPipelineLayout&&) = delete;
    RHIPipelineLayout& operator=(RHIPipelineLayout&&) = delete;

protected:
    RHIPipelineLayout() = default;
};

} // namespace rhi
