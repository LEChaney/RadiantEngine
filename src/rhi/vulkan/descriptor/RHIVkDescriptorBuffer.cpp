#include "RHIVkDescriptorBuffer.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"
#include "rhi/interface/descriptor/RHIDescriptorSet.h"
#include "fmt/format.h"

namespace rhi::vulkan {

UniquePtr<RHIVkDescriptorBuffer> RHIVkDescriptorBuffer::createUnique(RHIVkContext* ctx, const CreateInfo& ci) {
    return UniquePtr<RHIVkDescriptorBuffer>(new RHIVkDescriptorBuffer(ctx, ci));
}

RHIVkDescriptorBuffer::RHIVkDescriptorBuffer(RHIVkContext* ctx,
    const CreateInfo& ci)
    :  m_ctx(ctx)
{
    RHIBufferUsageFlags usage = ci.usage 
        | RHIBufferUsage::ShaderDeviceAddress; // required to query device address for binding descriptor buffer
    const RHIMemoryPropertyFlags memProps = RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent;
    m_buffer = m_ctx->createBuffer(ci.sizeBytes, usage, memProps);

    if (ci.persistentMapped && m_buffer) {
        m_mapped = m_buffer->map();
    }
}

RHIVkDescriptorBuffer::~RHIVkDescriptorBuffer() {
    if (m_buffer && m_mapped) {
        m_buffer->unmap();
        m_mapped = nullptr;
    }
    m_buffer.reset();
}

bool RHIVkDescriptorBuffer::allocateRaw(uint64 size, uint64 alignment, uint64& outOffset) {
    uint64 current = m_bump;
    uint64 aligned = (current + (alignment - 1)) & ~(alignment - 1);
    if (aligned + size > getSize()) {
        return false;
    }
    outOffset = aligned;
    m_bump = aligned + size;
    return true;
}

RHIDescriptorSet RHIVkDescriptorBuffer::allocateSet(RHIDescriptorSetLayout* layout,
    const std::string&)
{
    ASSERT(layout != nullptr && "RHIVkDescriptorBufferArena::allocateSet: layout must not be null");

    // Placeholder: assume layout can provide a byteSize() in future; use fixed size now
    VkDescriptorSetLayout vkLayout = static_cast<RHIVkDescriptorSetLayout*>(layout)->getVk();
    VkDeviceSize layoutSize = 0;
    vkGetDescriptorSetLayoutSizeEXT(m_ctx->getVkDevice(), vkLayout, &layoutSize);
    uint64 alignment = m_ctx->getVkDescriptorBufferProperties().descriptorBufferOffsetAlignment;
    uint64 offset = 0;
    ASSERT(allocateRaw(layoutSize, alignment, offset) && 
        "RHIVkDescriptorBufferArena::allocateSet: failed to allocate descriptor set");

    RHIDescriptorSet set {
        .buffer = this,
        .offset = offset,
        .size   = layoutSize,
        .layout = layout
    };
    return set;
}

void RHIVkDescriptorBuffer::resetLinear() {
    m_bump = 0;
}

} // namespace rhi::vulkan
