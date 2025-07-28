#include "rhivk_buffer.h"
#include "rhivk_context.h"

#include <cassert>
#include <cstring>

namespace rhi::vulkan {


UniquePtr<RHIVKBuffer> RHIVKBuffer::createUnique(RHIVKContext *context, uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) {
    return UniquePtr<RHIVKBuffer>(new RHIVKBuffer(context, size, usage, memProps));
}

RHIVKBuffer::RHIVKBuffer(RHIVKContext* context, uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps)
    : m_context(context)
    , m_size(size)
{
    // Assume context provides a VmaAllocator* via getVmaAllocator()
    VmaAllocator allocator = m_context->getVmaAllocator();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = 0;
    if ((usage & RHIBufferUsage::TransferSrc) == RHIBufferUsage::TransferSrc) {
        bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    if ((usage & RHIBufferUsage::TransferDst) == RHIBufferUsage::TransferDst) {
        bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    // ... add more as needed
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    if ((memProps & RHIMemoryProperty::HostVisible) == RHIMemoryProperty::HostVisible) {
        allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    if ((memProps & RHIMemoryProperty::HostCoherent) == RHIMemoryProperty::HostCoherent) {
        allocInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }

    VmaAllocationInfo vmaAllocInfo{};
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_buffer, &m_allocation, &vmaAllocInfo);
}

RHIVKBuffer::~RHIVKBuffer() {
    VmaAllocator allocator = m_context->getVmaAllocator();
    if (m_buffer && m_allocation) {
        vmaDestroyBuffer(allocator, m_buffer, m_allocation);
    }
}


void *RHIVKBuffer::map()
{
    void* data = nullptr;
    VmaAllocator allocator = m_context->getVmaAllocator();
    vmaMapMemory(allocator, m_allocation, &data);
    return data;
}

void RHIVKBuffer::unmap() {
    VmaAllocator allocator = m_context->getVmaAllocator();
    vmaUnmapMemory(allocator, m_allocation);
}

} // namespace rhi::vulkan
