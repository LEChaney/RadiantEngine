#pragma once
#include "rhi/rhi_buffer.h"
#include "rhi/rhi_buffer_usage.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKBuffer : public rhi::RHIBuffer {
public:
    RHIVKBuffer(VkDevice device, VkPhysicalDevice physicalDevice, uint64_t size, BufferUsage usage, MemoryProperty memProps);
    ~RHIVKBuffer() override;
    void* map() override;
    void unmap() override;
    VkBuffer get_vk() const { return m_buffer; }
private:
    VkDevice m_device;
    VkBuffer m_buffer;
    VkDeviceMemory m_memory;
    uint64_t m_size;
};

} // namespace rhi::vulkan
