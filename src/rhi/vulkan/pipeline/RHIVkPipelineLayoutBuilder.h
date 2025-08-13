#pragma once

namespace rhi { struct RHIPipelineLayoutBuilderOps; }

namespace rhi::vulkan {
    
// Returns the Vulkan ops for the value-type RHIPipelineLayoutBuilder
const RHIPipelineLayoutBuilderOps* getVkPipelineLayoutBuilderOps();

} // namespace rhi::vulkan
