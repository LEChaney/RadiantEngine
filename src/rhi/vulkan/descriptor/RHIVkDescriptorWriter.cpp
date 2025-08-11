#include "RHIVkDescriptorWriter.h"

namespace rhi::vulkan {

UniquePtr<RHIVkDescriptorWriter> RHIVkDescriptorWriter::createUnique(
    const RHIDescriptorSetAllocation& alloc) 
{
    return UniquePtr<RHIVkDescriptorWriter>(new RHIVkDescriptorWriter(alloc));
}

RHIVkDescriptorWriter::RHIVkDescriptorWriter(const RHIDescriptorSetAllocation& alloc)
    : RHIDescriptorWriter(alloc) {}

} // namespace rhi::vulkan