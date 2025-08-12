
#pragma once
#include "rhi/interface/descriptor/RHIDescriptorSetLayout.h"
#include "core/CoreDefs.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi::vulkan {

class RHIVkContext;

class RHIVkDescriptorSetLayout : public RHIDescriptorSetLayout {
public:
    static UniquePtr<RHIVkDescriptorSetLayout> createUnique(RHIVkContext* context, VkDescriptorSetLayout layout);

    ~RHIVkDescriptorSetLayout() override;

    RHIVkDescriptorSetLayout(const RHIVkDescriptorSetLayout&) = delete;
    RHIVkDescriptorSetLayout& operator=(const RHIVkDescriptorSetLayout&) = delete;
    RHIVkDescriptorSetLayout(RHIVkDescriptorSetLayout&&) = delete;
    RHIVkDescriptorSetLayout& operator=(RHIVkDescriptorSetLayout&&) = delete;

    VkDescriptorSetLayout getVk() const { return m_layout; }

private:
    RHIVkDescriptorSetLayout(RHIVkContext* context, VkDescriptorSetLayout layout);
    
    RHIVkContext* m_context = nullptr;
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
};

} // namespace rhi::vulkan
