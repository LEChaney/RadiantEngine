#include "rhivk_buffer.h"
#include <cassert>
#include <cstring>

namespace rhi::vulkan {

RHIVKBuffer::RHIVKBuffer(VkDevice device, VkPhysicalDevice physicalDevice, uint64_t size, BufferUsage usage, MemoryProperty memProps)
    : m_device(device), m_size(size)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = 0;
    if ((usage & BufferUsage::TransferSrc) == BufferUsage::TransferSrc) bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if ((usage & BufferUsage::TransferDst) == BufferUsage::TransferDst) bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    // ... add more as needed
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device, &bufferInfo, nullptr, &m_buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, m_buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    bool found = false;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            ((memProps & MemoryProperty::HostVisible) == MemoryProperty::HostVisible ? (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) : true) &&
            ((memProps & MemoryProperty::HostCoherent) == MemoryProperty::HostCoherent ? (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) : true)) {
            allocInfo.memoryTypeIndex = i;
            found = true;
            break;
        }
    }
    assert(found && "No suitable memory type found for buffer");
    vkAllocateMemory(device, &allocInfo, nullptr, &m_memory);
    vkBindBufferMemory(device, m_buffer, m_memory, 0);
}

RHIVKBuffer::~RHIVKBuffer() {
    if (m_buffer) vkDestroyBuffer(m_device, m_buffer, nullptr);
    if (m_memory) vkFreeMemory(m_device, m_memory, nullptr);
}

void* RHIVKBuffer::map() {
    void* data = nullptr;
    vkMapMemory(m_device, m_memory, 0, m_size, 0, &data);
    return data;
}

void RHIVKBuffer::unmap() {
    vkUnmapMemory(m_device, m_memory);
}

} // namespace rhi::vulkan
