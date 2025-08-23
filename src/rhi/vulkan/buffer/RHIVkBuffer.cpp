#include "RHIVkBuffer.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"

#include <cstring>

namespace RHI::Vulkan {


UniquePtr<RHIVkBuffer> RHIVkBuffer::createUnique(RHIVkContext *context, uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) {
    return UniquePtr<RHIVkBuffer>(new RHIVkBuffer(context, size, usage, memProps));
}

RHIVkBuffer::RHIVkBuffer(RHIVkContext* context, uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps)
    : RHIBuffer(size, usage), m_ctx(context)
{
    VmaAllocator allocator = m_ctx->getVmaAllocator();

    VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = toVkBufferUsageFlags(usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.requiredFlags = toVkMemoryPropertyFlags(memProps);
    if (memProps.hasFlag(RHIMemoryProperty::HostVisible)) {
        allocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT; // Needed to allow mapping when using VMA_MEMORY_USAGE_AUTO
    }

    VmaAllocationInfo vmaAllocInfo{};
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_buffer, &m_allocation, &vmaAllocInfo);
}

RHIVkBuffer::~RHIVkBuffer() {
    VmaAllocator allocator = m_ctx->getVmaAllocator();
    if (m_buffer && m_allocation) {
        unmap(); // Ensure unmapping before destruction
        vmaDestroyBuffer(allocator, m_buffer, m_allocation);
    }
}

void *RHIVkBuffer::map()
{
    if (m_mapped) {
        return m_mapped; // Already mapped
    }
    
    VmaAllocator allocator = m_ctx->getVmaAllocator();
    vmaMapMemory(allocator, m_allocation, &m_mapped);
    return m_mapped;
}

void RHIVkBuffer::unmap() {
    if (!m_mapped) {
        return; // Not mapped
    }

    VmaAllocator allocator = m_ctx->getVmaAllocator();
    vmaUnmapMemory(allocator, m_allocation);
    m_mapped = nullptr; // Clear mapped pointer
}

uint64 RHIVkBuffer::getDeviceAddress() const {
    VkBufferDeviceAddressInfo addrInfoQuery{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfoQuery.buffer = m_buffer;
    return vkGetBufferDeviceAddress(m_ctx->getVkDevice(), &addrInfoQuery);
}

} // namespace rhi::vulkan
