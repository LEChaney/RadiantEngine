#include "RHIVkDescriptorBufferArena.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"
#include "rhi/interface/buffer/RHIBuffer.h"
#include "fmt/format.h"

namespace rhi::vulkan {

UniquePtr<RHIVkDescriptorBufferArena> RHIVkDescriptorBufferArena::createUnique(RHIVkContext* ctx, const CreateInfo& ci) {
    return UniquePtr<RHIVkDescriptorBufferArena>(new RHIVkDescriptorBufferArena(ctx, ci));
}

RHIVkDescriptorBufferArena::RHIVkDescriptorBufferArena(RHIVkContext* ctx, const CreateInfo& ci)
    : m_ctx(ctx), m_size(ci.sizeBytes) {
    // Create underlying buffer (placeholder usage & memory flags for now)
    // TODO: adjust usage flags for sampler / push descriptor usage
    const RHIBufferUsageFlags usage = ci.usage;
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
        return false;
    }
    outOffset = aligned;
    m_bump = aligned + size;
    return true;
}

RHIDescriptorSetAllocation RHIVkDescriptorBufferArena::allocateSet(RHIDescriptorSetLayout* layout, const char* /*debugName*/) {
    if (!layout) {
        throw std::invalid_argument("RHIVkDescriptorBufferArena::allocateSet: layout cannot be null");
    }
    // Placeholder: assume layout can provide a byteSize() in future; use fixed size now
    VkDescriptorSetLayout vkLayout = static_cast<RHIVkDescriptorSetLayout*>(layout)->getVk();
    VkDeviceSize layoutSize = 0;
    vkGetDescriptorSetLayoutSizeEXT(m_ctx->getVkDevice(), vkLayout, &layoutSize);
    uint64 alignment = m_ctx->getVkDescriptorBufferProperties().descriptorBufferOffsetAlignment;
    uint64 offset = 0;
    if (!allocateRaw(layoutSize, alignment, offset)) {
        throw std::runtime_error("RHIVkDescriptorBufferArena::allocateSet: failed to allocate descriptor set");
    }
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

} // namespace rhi::vulkan
