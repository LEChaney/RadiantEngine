
#include "rhi_image_utils.h"
#include "rhi/rhi_context.h"
#include "rhi/rhi_image.h"
#include "rhi/rhi_buffer.h"
#include "rhi/rhi_buffer_usage.h"
#include "rhi/rhi_command_buffer.h"
#include "rhi/rhi_queue.h"
#include "rhi/rhi_image_layout.h"
#include <vector>
#include <cstring>

namespace rhi {

bool read_image_to_cpu(RHIContext* context, RHIImage* image, uint32_t width, uint32_t height, std::vector<uint8_t>& outData) {
    const size_t imageSize = width * height * 4;
    // 1. Create a host-visible staging buffer
    auto stagingBuffer = context->create_buffer(
        imageSize,
        BufferUsage::TransferDst,
        MemoryProperty::HostVisible | MemoryProperty::HostCoherent
    );

    // 2. Create and begin a command buffer
    auto cmd = context->create_command_buffer();
    cmd->begin();

    // 3. Transition image layout for copy
    cmd->transition_image_layout(image, ImageLayout::TransferSrc);

    // 4. Copy image to buffer
    cmd->copy_image_to_buffer(image, stagingBuffer.get(), width, height);

    // 5. End and submit command buffer, wait for completion
    cmd->end();
    context->get_graphics_queue()->submit_and_wait(cmd.get());

    // 6. Map buffer and read data
    outData.resize(imageSize);
    void* data = stagingBuffer->map();
    std::memcpy(outData.data(), data, imageSize);
    stagingBuffer->unmap();

    return true;
}

} // namespace rhi
