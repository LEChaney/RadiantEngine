
#pragma once
#include "rhi/rhi_descriptor_set_layout.h"
#include "rhi/vulkan/rhivk_core_defs.h"
#include "core/core_defs.h"
#include <vulkan/vulkan.h>

namespace rhi::vulkan {

class RHIVKContext;

class RHIVKDescriptorSetLayout : public rhi::RHIDescriptorSetLayout {
public:
    static UniquePtr<RHIVKDescriptorSetLayout> createUnique(RHIVKContext* context, VkDescriptorSetLayout layout);

    ~RHIVKDescriptorSetLayout() override;

    RHIVKDescriptorSetLayout(const RHIVKDescriptorSetLayout&) = delete;
    RHIVKDescriptorSetLayout& operator=(const RHIVKDescriptorSetLayout&) = delete;
    RHIVKDescriptorSetLayout(RHIVKDescriptorSetLayout&&) = delete;
    RHIVKDescriptorSetLayout& operator=(RHIVKDescriptorSetLayout&&) = delete;

    VkDescriptorSetLayout getVkDescriptorSetLayout() const { return m_layout; }

private:
    RHIVKDescriptorSetLayout(RHIVKContext* context, VkDescriptorSetLayout layout);
    
    RHIVKContext* m_context = nullptr;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
};

} // namespace rhi::vulkan
