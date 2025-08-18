#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/command/RHIVkCommandBuffer.h"
#include "rhi/interface/descriptor/RHIDescriptorSet.h"
#include "rhi/vulkan/image/RHIVkImage.h"
#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/buffer/RHIVkBuffer.h"
#include "rhi/vulkan/pipeline/RHIVkPipeline.h"
#include "rhi/vulkan/pipeline/RHIVkPipelineLayout.h"
#include "rhi/vulkan/core/RHIVulkanInclude.h"
#include "rhi/vulkan/core/RHIVkTypeConversion.h"
#include <limits>

namespace RHI::Vulkan {


UniquePtr<RHIVkCommandBuffer> RHIVkCommandBuffer::createUnique(RHIVkContext* context) {
    return UniquePtr<RHIVkCommandBuffer>(new RHIVkCommandBuffer(context));
}

RHIVkCommandBuffer::RHIVkCommandBuffer(RHIVkContext* context)
    : m_context(context) 
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = context->getVkCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(context->getVkDevice(), &allocInfo, &m_vkCmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer");
    }
}

RHIVkCommandBuffer::~RHIVkCommandBuffer() {
    if (m_vkCmd && m_context) {
        vkFreeCommandBuffers(
            m_context->getVkDevice(), 
            m_context->getVkCommandPool(), 
            1, 
            &m_vkCmd);
    }
}

void RHIVkCommandBuffer::begin()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(m_vkCmd, &beginInfo);
}

void RHIVkCommandBuffer::end() {
    vkEndCommandBuffer(m_vkCmd);

    // After ending the command buffer, we can flush the tracked image layout states,
    // and store the last known layouts on the images.
    for (const auto& [image, layout] : m_trackedImageLayouts) {
        // TODO: Thread safety: How should this be handled? The image storage is really just convenience for tracking.
        // In a multi-threaded context, we might just not want to do this.
        image->lastLayout = layout;
    }
    m_trackedImageLayouts.clear();
}

void RHIVkCommandBuffer::reset()
{
    m_boundPipeline = nullptr;
    m_boundDescriptorBuffers.clear();
    m_boundDescriptorBuffersToIndex.clear();
    m_trackedImageLayouts.clear();
    vkResetCommandBuffer(m_vkCmd, 0);
}

void RHIVkCommandBuffer::clearColor(RHIImage* image, const glm::vec4& color)
{
    VkImage vkImage = static_cast<RHIVkImage*>(image)->getVk();
    VkClearColorValue clearColor{};
    clearColor.float32[0] = color.r;
    clearColor.float32[1] = color.g;
    clearColor.float32[2] = color.b;
    clearColor.float32[3] = color.a;
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    vkCmdClearColorImage(m_vkCmd, vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
}

void RHIVkCommandBuffer::transitionImageLayout(RHIImage *image, RHIImageLayout newLayout)
{
    RHIImageLayout oldLayout;
    if (m_trackedImageLayouts.contains(image)) {
        oldLayout = m_trackedImageLayouts[image];
    } else {
        // TODO: Assert if not main thread, this is not thread-safe
        oldLayout = image->lastLayout;
    }

    transitionImageLayout(image, oldLayout, newLayout);
}

void RHIVkCommandBuffer::transitionImageLayout(RHIImage* image, RHIImageLayout oldLayout, RHIImageLayout newLayout) {
    if (oldLayout == newLayout) {
        // No transition needed
        return;
    }

    VkImage vkImage = static_cast<RHIVkImage*>(image)->getVk();
    auto mapStageAccess = [](RHIImageLayout layout, VkPipelineStageFlags2& stage, VkAccessFlags2& access) {
        switch (layout) {
        case RHIImageLayout::Undefined:
            stage = 0; access = 0; break;
        case RHIImageLayout::TransferDst:
            stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT; access = VK_ACCESS_2_TRANSFER_WRITE_BIT; break;
        case RHIImageLayout::TransferSrc:
            stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT; access = VK_ACCESS_2_TRANSFER_READ_BIT; break;
        case RHIImageLayout::ColorAttachment:
            stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT; break;
        case RHIImageLayout::Present:
            stage = VK_PIPELINE_STAGE_2_NONE; access = 0; break; // Present doesn't require access mask
        case RHIImageLayout::General:
            stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; access = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT; break;
        default:
            stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; access = 0; break;
        }
    };

    VkPipelineStageFlags2 srcStage{}, dstStage{};
    VkAccessFlags2 srcAccess{}, dstAccess{};
    mapStageAccess(oldLayout, srcStage, srcAccess);
    mapStageAccess(newLayout, dstStage, dstAccess);

    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = srcStage ? srcStage : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage ? dstStage : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = toVkImageLayout(oldLayout);
    barrier.newLayout = toVkImageLayout(newLayout);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = vkImage;
    bool isDepth = image->getFormat() == RHIFormat::RHI_FORMAT_D32_SFLOAT
                || image->getFormat() == RHIFormat::RHI_FORMAT_D32_SFLOAT_S8_UINT;
    barrier.subresourceRange.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(m_vkCmd, &depInfo);

    m_trackedImageLayouts[image] = newLayout;
}

void RHIVkCommandBuffer::copyImageToBuffer(RHI::RHIImage* image, RHI::RHIBuffer* buffer, uint32 width, uint32 height) {
    VkImage vkImage = static_cast<RHIVkImage*>(image)->getVk();
    VkBuffer vkBuffer = static_cast<RHIVkBuffer*>(buffer)->getVk();
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(m_vkCmd, vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkBuffer, 1, &region);
}

void RHIVkCommandBuffer::bindVkPipeline(VkPipelineBindPoint bindPoint, RHIPipeline* pipeline) {
    auto vkPipe = static_cast<RHIVkPipeline*>(pipeline);
    vkCmdBindPipeline(m_vkCmd, bindPoint, vkPipe->getVk());
}

void RHIVkCommandBuffer::bindComputePipeline(RHIPipeline* pipeline) {
    bindVkPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

void RHIVkCommandBuffer::bindGraphicsPipeline(RHIPipeline* pipeline) {
    bindVkPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void RHIVkCommandBuffer::bindDescriptorBuffers(const Array<RHIDescriptorBuffer*>& descBuffers) {
    if (descBuffers == m_boundDescriptorBuffers) {
        return; // Buffer set already bound, no need to do anything
    }

    Array<VkDescriptorBufferBindingInfoEXT> bindingInfos;
    bindingInfos.resize(descBuffers.size());
    for (int32 i = 0; i < descBuffers.size(); ++i) {
        VkBufferDeviceAddressInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferInfo.buffer = static_cast<RHIVkBuffer*>(descBuffers[i]->getBuffer())->getVk();
        VkDeviceAddress address = vkGetBufferDeviceAddress(m_context->getVkDevice(), &bufferInfo);

        bindingInfos[i].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
        bindingInfos[i].usage = toVkBufferUsageFlags(descBuffers[i]->getUsage());
        bindingInfos[i].address = address;

        // Create a mapping so we can look up a bound descriptor buffer's binding index
        m_boundDescriptorBuffersToIndex[descBuffers[i]] = i;
    }

    vkCmdBindDescriptorBuffersEXT(
        m_vkCmd,
        bindingInfos.size(),
        bindingInfos.data()
    );

    m_boundDescriptorBuffers = descBuffers;
}
void RHIVkCommandBuffer::bindDescriptorSets(const Array<RHIDescriptorSetBinding>& setBindings,
    RHIPipelineLayout* pipelineLayout, RHIPipelineBindPoint bindPoint)
{
    SmallArray<uint32, 8> bufferIndices;
    SmallArray<VkDeviceSize, 8> setOffsetsInBuffer;
    constexpr uint32 kInvalidSetIndex = std::numeric_limits<uint32>::max();
    uint32 prevSetIndex = kInvalidSetIndex; // Max uint32
    for (const auto& [setIndex, set] : setBindings) {
    if (prevSetIndex == kInvalidSetIndex) {
            prevSetIndex = setIndex - 1;
        }
        ASSERT(setIndex == prevSetIndex + 1 && "Descriptor set indices must be contiguous");
        ASSERT(set.getBuffer() && "Descriptor buffer is NULL");
        ASSERT(m_boundDescriptorBuffersToIndex.contains(set.getBuffer()) && "Descriptor buffer not bound");
        bufferIndices.push_back(m_boundDescriptorBuffersToIndex[set.getBuffer()]);
        setOffsetsInBuffer.push_back(set.getOffset());
        prevSetIndex = setIndex;
    }

    auto vKPipelineLayout = static_cast<RHIVkPipelineLayout*>(pipelineLayout);
    vkCmdSetDescriptorBufferOffsetsEXT(
        m_vkCmd,
        toVkPipelineBindPoint(bindPoint),
        vKPipelineLayout->getVk(),
        setBindings[0].setIndex,
        setBindings.size(),
        bufferIndices.data(),
        setOffsetsInBuffer.data()
    );
}
void RHIVkCommandBuffer::pushConstants(RHIPipelineLayout* layout,
    RHIShaderStageFlags shaderStageFlags, uint32 offset, uint32 size, const void* data)
{
    vkCmdPushConstants(
        m_vkCmd,
        static_cast<RHIVkPipelineLayout*>(layout)->getVk(),
        toVkShaderStageFlags(shaderStageFlags),
        offset,
        size,
        data
    );
}
void RHIVkCommandBuffer::dispatchCompute(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ) {
    vkCmdDispatch(m_vkCmd, groupCountX, groupCountY, groupCountZ);
}

void RHIVkCommandBuffer::dispatchMesh(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ) {
    vkCmdDrawMeshTasksEXT(m_vkCmd, groupCountX, groupCountY, groupCountZ);
}

void RHIVkCommandBuffer::beginDynRendering(const RHIRenderingInfo& info) {
    // Build color attachment infos
    SmallArray<VkRenderingAttachmentInfo, 4> colorAtts;
    colorAtts.reserve(info.colorAttachments.size());
    for (const auto& att : info.colorAttachments) {
        VkRenderingAttachmentInfo colorAtt{};
        colorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAtt.imageView = static_cast<RHIVkImageView*>(att.view)->getVk();
        colorAtt.imageLayout = toVkImageLayout(att.layout);
        // TODO: Expose load/store ops
        colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.clearValue.color = {0.0f, 0.0f, 0.0f, 1.0f};
        colorAtts.push_back(colorAtt);
    }

    // Depth
    VkRenderingAttachmentInfo depthAtt{};
    if (info.depthAttachment.view) {
        depthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAtt.imageView = static_cast<RHIVkImageView*>(info.depthAttachment.view)->getVk();
        depthAtt.imageLayout = toVkImageLayout(info.depthAttachment.layout);
        // TODO: Expose load/store ops
        depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAtt.clearValue.depthStencil = { 0.0f, 0};
    }

    // Separate Stencil
    VkRenderingAttachmentInfo stencilAtt{};
    if (info.stencilAttachment.view) {
        stencilAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        stencilAtt.imageView = static_cast<RHIVkImageView*>(info.stencilAttachment.view)->getVk();
        stencilAtt.imageLayout = toVkImageLayout(info.stencilAttachment.layout);
        // TODO: Expose load/store ops
        stencilAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        stencilAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        stencilAtt.clearValue.depthStencil.stencil = 0;
    }

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = { info.renderArea.x, info.renderArea.y };
    renderingInfo.renderArea.extent = { info.renderArea.width, info.renderArea.height };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = static_cast<uint32>(colorAtts.size());
    renderingInfo.pColorAttachments = colorAtts.empty() ? nullptr : colorAtts.data();
    renderingInfo.pDepthAttachment = depthAtt.imageView ? &depthAtt : nullptr;
    renderingInfo.pStencilAttachment = stencilAtt.imageView ? &stencilAtt : nullptr;
    vkCmdBeginRendering(m_vkCmd, &renderingInfo);
}

void RHIVkCommandBuffer::endDynRendering() {
    vkCmdEndRendering(m_vkCmd);
}

void RHIVkCommandBuffer::setViewport(const RHIViewport& viewport) {
    VkViewport vkViewport{};
    vkViewport.x = viewport.viewRect.x;
    vkViewport.y = viewport.viewRect.y;
    vkViewport.width = viewport.viewRect.width;
    vkViewport.height = viewport.viewRect.height;
    vkViewport.minDepth = viewport.minDepth;
    vkViewport.maxDepth = viewport.maxDepth;
    vkCmdSetViewport(m_vkCmd, 0, 1, &vkViewport);
}

void RHIVkCommandBuffer::setScissor(const RHIRect2D& scissorRect) {
    VkRect2D vkScissorRect{};
    vkScissorRect.offset = { scissorRect.x, scissorRect.y };
    vkScissorRect.extent = { scissorRect.width, scissorRect.height };
    vkCmdSetScissor(m_vkCmd, 0, 1, &vkScissorRect);
}

} // namespace rhi::vulkan
