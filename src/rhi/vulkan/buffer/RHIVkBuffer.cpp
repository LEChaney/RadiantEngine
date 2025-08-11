#include "RHIVkBuffer.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"

#include <cstring>

namespace rhi::vulkan {


UniquePtr<RHIVkBuffer> RHIVkBuffer::createUnique(RHIVkContext *context, uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) {
    return UniquePtr<RHIVkBuffer>(new RHIVkBuffer(context, size, usage, memProps));
}

RHIVkBuffer::RHIVkBuffer(RHIVkContext* context, uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps)
    : RHIBuffer(size, usage), m_context(context) 
{
    VmaAllocator allocator = m_context->getVmaAllocator();

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
    VmaAllocator allocator = m_context->getVmaAllocator();
    if (m_buffer && m_allocation) {
        vmaDestroyBuffer(allocator, m_buffer, m_allocation);
    }
}


void *RHIVkBuffer::map()
{
    void* data = nullptr;
    VmaAllocator allocator = m_context->getVmaAllocator();
    vmaMapMemory(allocator, m_allocation, &data);
    return data;
}

void RHIVkBuffer::unmap() {
    VmaAllocator allocator = m_context->getVmaAllocator();
    vmaUnmapMemory(allocator, m_allocation);
}

} // namespace rhi::vulkan
