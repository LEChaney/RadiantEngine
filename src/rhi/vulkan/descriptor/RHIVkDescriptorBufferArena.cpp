#include "RHIVkDescriptorBufferArena.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"
#include "rhi/interface/buffer/RHIBuffer.h"
#include "fmt/format.h"

namespace rhi::vulkan {

UniquePtr<RHIVkDescriptorBufferArena> RHIVkDescriptorBufferArena::createUnique(RHIVkContext* ctx, const CreateInfo& ci) {
    return UniquePtr<RHIVkDescriptorBufferArena>(new RHIVkDescriptorBufferArena(ctx, ci));
}

RHIVkDescriptorBufferArena::RHIVkDescriptorBufferArena(RHIVkContext* ctx,
    const CreateInfo& ci)
    : m_ctx(ctx), m_size(ci.sizeBytes) {
    RHIBufferUsageFlags usage = ci.usage;
    usage |= RHIBufferUsage::ShaderDeviceAddress; // required to query device address for binding descriptor buffer
    const RHIMemoryPropertyFlags memProps = RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent;
    m_buffer = m_ctx->createBuffer(m_size, usage, memProps);
    if (ci.persistentMapped && m_buffer) {
        m_mapped = m_buffer->map();
    }
}

RHIVkDescriptorBufferArena::~RHIVkDescriptorBufferArena() {
    if (m_buffer && m_mapped) {
        m_buffer->unmap();
        m_mapped = nullptr;
    }
    m_buffer.reset();
}

bool RHIVkDescriptorBufferArena::allocateRaw(size_t size, size_t alignment, size_t& outOffset) {
    size_t current = m_bump;
    size_t aligned = (current + (alignment - 1)) & ~(alignment - 1);
    if (aligned + size > m_size) {
        ASSERT(false && "RHIVkDescriptorBufferArena: Out of space for descriptor set allocation");
        return false;
    }
    outOffset = aligned;
    m_bump = aligned + size;
    return true;
}

RHIDescriptorSetAllocation RHIVkDescriptorBufferArena::allocateSet(RHIDescriptorSetLayout* layout, const char* /*debugName*/) {
    ASSERT(layout != nullptr && "RHIVkDescriptorBufferArena::allocateSet: layout must not be null");

    // Placeholder: assume layout can provide a byteSize() in future; use fixed size now
    VkDescriptorSetLayout vkLayout = static_cast<RHIVkDescriptorSetLayout*>(layout)->getVk();
    VkDeviceSize layoutSize = 0;
    vkGetDescriptorSetLayoutSizeEXT(m_ctx->getVkDevice(), vkLayout, &layoutSize);
    uint64 alignment = m_ctx->getVkDescriptorBufferProperties().descriptorBufferOffsetAlignment;
    uint64 offset = 0;
    ASSERT(allocateRaw(layoutSize, alignment, offset) && 
        "RHIVkDescriptorBufferArena::allocateSet: failed to allocate descriptor set");

    RHIDescriptorSetAllocation alloc {
        .arena = this,
        .offset = offset,
        .size = layoutSize,
        .layout = layout
    };
    return alloc;
}

void RHIVkDescriptorBufferArena::resetLinear() {
    m_bump = 0;
}

bool RHIVkDescriptorBufferArena::isValidAddress(void* ptr) const {
    if (!m_mapped) {
        return false; // Not mapped, cannot validate
    }
    return ptr >= m_mapped && ptr < static_cast<uint8*>(m_mapped) + m_size;
}

bool RHIVkDescriptorBufferArena::isValidRange(void* ptr, size_t size) const {
    if (!m_mapped || size == 0 || !ptr) {
        return false;
    }
    auto* base = static_cast<uint8*>(m_mapped);
    auto* p    = static_cast<uint8*>(ptr);
    return !(p < base || p + size > base + m_size);
}

} // namespace rhi::vulkan
