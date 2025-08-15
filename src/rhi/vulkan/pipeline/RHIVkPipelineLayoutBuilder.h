#pragma once

namespace RHI { struct RHIPipelineLayoutBuilderOps; }

namespace RHI::Vulkan {
    
// Returns the Vulkan ops for the value-type RHIPipelineLayoutBuilder
const RHIPipelineLayoutBuilderOps* getVkPipelineLayoutBuilderOps();

} // namespace rhi::vulkan
