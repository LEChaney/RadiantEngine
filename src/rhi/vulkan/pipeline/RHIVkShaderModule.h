#pragma once
#include "rhi/interface/pipeline/RHIShaderModule.h"
#include "core/CoreDefs.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"

namespace rhi::vulkan {

class RHIVkContext;

class RHIVkShaderModule : public RHIShaderModule {
public:
    static UniquePtr<RHIVkShaderModule> createUnique(RHIVkContext* context, const std::string& spvFilePath);
    static UniquePtr<RHIVkShaderModule> createUnique(RHIVkContext* context, const Array<uint32>& shaderCode);

    ~RHIVkShaderModule() override;
    
    // Non-copyable, non-movable
    RHIVkShaderModule(const RHIVkShaderModule&) = delete;
    RHIVkShaderModule& operator=(const RHIVkShaderModule&) = delete;
    RHIVkShaderModule(RHIVkShaderModule&&) = delete;
    RHIVkShaderModule& operator=(RHIVkShaderModule&&) = delete;
    
    VkShaderModule getVk() const { return m_module; }
    
private:
    RHIVkShaderModule(RHIVkContext* context, const Array<uint32>& shaderCode);

    RHIVkContext* m_context = nullptr;
    VkShaderModule m_module = VK_NULL_HANDLE;
};

} // namespace rhi::vulkan
