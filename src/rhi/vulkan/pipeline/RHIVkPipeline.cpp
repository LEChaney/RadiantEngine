#include "RHIVkPipeline.h"
#include "RHIVkShaderModule.h"
#include "RHIVkPipelineLayout.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"

namespace RHI::Vulkan {

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
    ASSERT(desc.layout && desc.meshShader && desc.fragmentShader && "Graphics pipeline descriptor must have layout, mesh shader, fragment shader");

    // Stage infos: mesh + fragment
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_MESH_BIT_EXT; // Mesh shader stage
    stages[0].module = static_cast<RHIVkShaderModule*>(desc.meshShader)->getVk();
    stages[0].pName  = "main";
    stages[0].flags  = 0; // Could add specialization in future

    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = static_cast<RHIVkShaderModule*>(desc.fragmentShader)->getVk();
    stages[1].pName  = "main";

    // Fixed function structures (many unused with mesh shaders)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1; // Dynamic
    viewportState.scissorCount  = 1; // Dynamic

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT
        | VK_COLOR_COMPONENT_B_BIT
        | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.depthTestEnable = desc.depthWriteEnable;
    ds.depthWriteEnable = desc.depthWriteEnable;
    ds.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable = VK_FALSE;
    ds.front = {};
    ds.back = {};
    ds.minDepthBounds = 0.0f;
    ds.maxDepthBounds = 1.0f;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;

    // Dynamic states: viewport + scissor so we can set them at command time without recreating pipeline
    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = static_cast<uint32_t>(std::size(dynStates));
    dyn.pDynamicStates = dynStates;

    // (Optional) Depth stencil omitted (no depth buffer in simple swapchain render yet)
    // TODO: Handle depth / stencil

    VkFormat vkColorFormat = toVkFormat(desc.colorFormat);
    VkFormat vkDepthFormat = toVkFormat(desc.depthFormat);
    VkFormat vkStencilFormat = toVkFormat(desc.stencilFormat);
    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &vkColorFormat;
    rendering.depthAttachmentFormat = vkDepthFormat;
    rendering.stencilAttachmentFormat = vkStencilFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &rendering;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &ms;
    pipelineInfo.pDepthStencilState = &ds;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.pDynamicState = &dyn;
    pipelineInfo.layout = static_cast<RHIVkPipelineLayout*>(desc.layout)->getVk();
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT; // allow descriptor buffer usage

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult res = vkCreateGraphicsPipelines(context->getVkDevice(), VK_NULL_HANDLE,
        1, &pipelineInfo, nullptr, &pipeline);
    VK_CHECK(res);
    return UniquePtr<RHIVkPipeline>(new RHIVkPipeline(context, pipeline));
}

UniquePtr<RHIVkPipeline> RHIVkPipeline::createUniqueCompute(
    RHIVkContext* context, const RHIComputePipelineDescriptor& desc) 
{
    ASSERT(desc.layout && desc.computeShader && "Compute pipeline descriptor must have layout and compute shader");

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
    VK_CHECK(vkCreateComputePipelines(context->getVkDevice(), VK_NULL_HANDLE, 1,
        &pipelineInfo, nullptr, &pipeline));

    return UniquePtr<RHIVkPipeline>(new RHIVkPipeline(context, pipeline));
}

UniquePtr<RHIVkPipeline> RHIVkPipeline::createUniqueRayTracing(
    RHIVkContext* context, const RHIRayTracingPipelineDescriptor& desc) 
{
    throw std::runtime_error("Ray tracing pipelines not implemented yet");
}

} // namespace rhi::vulkan