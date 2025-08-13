#pragma once

namespace rhi {
struct RHIDescriptorSetLayoutBuilderOps;
}

namespace rhi::vulkan {

// Returns the Vulkan ops for the value-type RHIDescriptorSetLayoutBuilder
const RHIDescriptorSetLayoutBuilderOps* getVkDescriptorSetLayoutBuilderOps();

} // namespace rhi::vulkan
