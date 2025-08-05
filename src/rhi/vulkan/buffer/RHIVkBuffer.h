#pragma once
#include "rhi/interface/buffer/RHIBuffer.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include "core/CoreDefs.h"
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVkContext;

class RHIVkBuffer : public RHIBuffer {
public:
    static UniquePtr<RHIVkBuffer> createUnique(RHIVkContext* context, uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps);
    ~RHIVkBuffer() override;

    RHIVkBuffer(const RHIVkBuffer&) = delete;
    RHIVkBuffer& operator=(const RHIVkBuffer&) = delete;
    RHIVkBuffer(RHIVkBuffer&&) = delete;
    RHIVkBuffer& operator=(RHIVkBuffer&&) = delete;

    void* map() override;
    void unmap() override;

    VkBuffer getVk() const { return m_buffer; }

private:
    RHIVkBuffer(RHIVkContext* context, uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps);
    
    RHIVkContext* m_context = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
};

} // namespace rhi::vulkan
