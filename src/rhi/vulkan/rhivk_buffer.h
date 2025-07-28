#pragma once
#include "rhi/rhi_buffer.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include "core/core_defs.h"
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKBuffer : public RHIBuffer {
public:
    static UniquePtr<RHIVKBuffer> createUnique(RHIVKContext* context, uint64 size, RHIBufferUsage usage, RHIMemoryProperty memProps);
    ~RHIVKBuffer() override;

    void* map() override;
    void unmap() override;

    VkBuffer getVk() const { return m_buffer; }

protected:
    RHIVKBuffer(RHIVKContext* context, uint64 size, RHIBufferUsage usage, RHIMemoryProperty memProps);
    RHIVKBuffer(const RHIVKBuffer&) = delete;
    RHIVKBuffer& operator=(const RHIVKBuffer&) = delete;
    RHIVKBuffer(RHIVKBuffer&&) = delete;
    RHIVKBuffer& operator=(RHIVKBuffer&&) = delete;

private:
    RHIVKContext* m_context = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    uint64 m_size = 0;
};

} // namespace rhi::vulkan
