#include "RHIVkBuffer.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"

#include <cstring>

namespace rhi::vulkan {


UniquePtr<RHIVkBuffer> RHIVkBuffer::createUnique(RHIVkContext *context, uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) {
    return UniquePtr<RHIVkBuffer>(new RHIVkBuffer(context, size, usage, memProps));
}

RHIVkBuffer::RHIVkBuffer(RHIVkContext* context, uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps)
    : RHIBuffer(size)
    , m_context(context)
{
    // Assume context provides a VmaAllocator* via getVmaAllocator()
    VmaAllocator allocator = m_context->getVmaAllocator();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = 0;
    bufferInfo.usage |= toVkBufferUsageFlags(usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // TODO: Allow specifying this on creation
    allocInfo.flags = 0;
    allocInfo.requiredFlags = toVkMemoryPropertyFlags(memProps);
    if ((memProps & RHIMemoryProperty::HostVisible) == RHIMemoryProperty::HostVisible) {
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
