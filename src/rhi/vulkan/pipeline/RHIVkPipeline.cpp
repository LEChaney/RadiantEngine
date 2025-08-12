#include "RHIVkPipeline.h"
#include "RHIVkShaderModule.h"
#include "RHIVkPipelineLayout.h"
#include "rhi/vulkan/core/RHIVkContext.h"

namespace rhi::vulkan {

RHIVkPipeline::RHIVkPipeline(RHIVkContext* context, VkPipeline pipeline) 
    : m_context(context), m_pipeline(pipeline) 
{

}

RHIVkPipeline::~RHIVkPipeline() 
{
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_context->getVkDevice(), m_pipeline, nullptr);
    }
}

UniquePtr<RHIVkPipeline> RHIVkPipeline::createUniqueGraphics(
    RHIVkContext* context, const RHIGraphicsPipelineDescriptor& desc) 
{
    throw std::runtime_error("Graphics pipelines not implemented yet");
}

UniquePtr<RHIVkPipeline> RHIVkPipeline::createUniqueCompute(
    RHIVkContext* context, const RHIComputePipelineDescriptor& desc) 
{
    if (!desc.layout || !desc.computeShader) {
        throw std::runtime_error("Compute pipeline descriptor must have layout and compute shader");
    }

    // Create the Vulkan pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = static_cast<RHIVkShaderModule*>(desc.computeShader)->getVk();
    pipelineInfo.stage.pName = "main"; // Entry point name
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
    pipelineInfo.layout = static_cast<RHIVkPipelineLayout*>(desc.layout)->getVk();
    VkPipeline pipeline;
    if (vkCreateComputePipelines(context->getVkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute pipeline");
    }

    return UniquePtr<RHIVkPipeline>(new RHIVkPipeline(context, pipeline));
}

UniquePtr<RHIVkPipeline> RHIVkPipeline::createUniqueRayTracing(
    RHIVkContext* context, const RHIRayTracingPipelineDescriptor& desc) 
{
    throw std::runtime_error("Ray tracing pipelines not implemented yet");
}

} // namespace rhi::vulkan