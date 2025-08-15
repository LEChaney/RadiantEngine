#pragma once

namespace RHI {
struct RHIDescriptorWriterOps;
}

namespace RHI::Vulkan {
// Returns the Vulkan backend ops table for RHIDescriptorWriter
const RHIDescriptorWriterOps* getVkDescriptorWriterOps();
} // namespace rhi::vulkan
