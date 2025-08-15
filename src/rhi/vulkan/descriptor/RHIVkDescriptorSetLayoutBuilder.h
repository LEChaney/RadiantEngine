#pragma once

namespace RHI {
struct RHIDescriptorSetLayoutBuilderOps;
}

namespace RHI::Vulkan {

// Returns the Vulkan ops for the value-type RHIDescriptorSetLayoutBuilder
const RHIDescriptorSetLayoutBuilderOps* getVkDescriptorSetLayoutBuilderOps();

} // namespace rhi::vulkan
