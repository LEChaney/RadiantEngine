
#include "rhi_image_utils.h"
#include "rhi/rhi_context.h"
#include "rhi/rhi_image.h"
#include "rhi/rhi_buffer.h"
#include "rhi/rhi_command_buffer.h"
#include "rhi/rhi_queue.h"
#include <vector>
#include <cstring>

namespace rhi {

bool readImageToCpu(
    RHIContext* context,
    RHIImage* image,
    uint32_t width,
    uint32_t height,
    Array<uint8>& outData,
    bool restoreLayout)
{
    const size_t imageSize = width * height * 4;
    // 1. Create a host-visible staging buffer
    auto stagingBuffer = context->createBuffer(
        imageSize,
        RHIBufferUsage::TransferDst,
        RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent
    );

    // 2. Create and begin a command buffer
    auto cmd = context->createCommandBuffer();
    cmd->begin();

    // 2.5 Save old image layout in case we need to restore it
    RHIImageLayout oldLayout = image->m_lastKnownLayout;

    // 3. Transition image layout for copy
    cmd->transitionImageLayout(image, RHIImageLayout::TransferSrc);

    // 4. Copy image to buffer
    cmd->copyImageToBuffer(image, stagingBuffer.get(), width, height);

    // 4.5 Optionally Restore original layout
    if (restoreLayout) {
        cmd->transitionImageLayout(image, oldLayout);
    }

    // 5. End and submit command buffer, wait for completion
    cmd->end();
    context->getGraphicsQueue()->submitAndWait(cmd.get());

    // 6. Map buffer and read data
    outData.resize(imageSize);
    void* data = stagingBuffer->map();
    std::memcpy(outData.data(), data, imageSize);
    stagingBuffer->unmap();

    return true;
}

} // namespace rhi
