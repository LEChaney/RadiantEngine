#include "RHIVkShaderModule.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include <fstream>
#include <ios>

namespace RHI::Vulkan {

UniquePtr<RHIVkShaderModule> RHIVkShaderModule::createUnique(RHIVkContext* context,
    const Path& spvFilePath)
{
    // Load SPIR-V shader code from file
    std::ifstream file(spvFilePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ASSERT(false && "Failed to open shader file");
        //throw std::runtime_error("Failed to open shader file: " + spvFilePath);
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    Array<uint32> shaderCode(fileSize / sizeof(uint32));
    if (!file.read(reinterpret_cast<char*>(shaderCode.data()), fileSize)) {
        ASSERT(false && "Failed to read shader file");
        //throw std::runtime_error("Failed to read shader file: " + spvFilePath);
        return nullptr;
    }

    return UniquePtr<RHIVkShaderModule>(new RHIVkShaderModule(context, shaderCode));
}

UniquePtr<RHIVkShaderModule> RHIVkShaderModule::createUnique(
    RHIVkContext* context, const Array<uint32>& shaderCode) 
{
    return UniquePtr<RHIVkShaderModule>(new RHIVkShaderModule(context, shaderCode));
}

RHIVkShaderModule::RHIVkShaderModule(RHIVkContext* context,
    const Array<uint32>& shaderCode)
    : m_context(context)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = shaderCode.size() * sizeof(uint32);
    createInfo.pCode = shaderCode.data();

    if (vkCreateShaderModule(context->getVkDevice(), &createInfo, nullptr, &m_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
}

RHIVkShaderModule::~RHIVkShaderModule() {
    if (m_module != VK_NULL_HANDLE && m_context) {
        vkDestroyShaderModule(m_context->getVkDevice(), m_module, nullptr);
    }
}

} // namespace rhi::vulkan
