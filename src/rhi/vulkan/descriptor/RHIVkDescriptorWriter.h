#pragma once

namespace rhi {
struct RHIDescriptorWriterOps;
}

namespace rhi::vulkan {
// Returns the Vulkan backend ops table for RHIDescriptorWriter
const RHIDescriptorWriterOps* getVkDescriptorWriterOps();
} // namespace rhi::vulkan
