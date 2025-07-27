
#include "rhi_image_utils.h"
#include "rhi/rhi_context.h"
#include "rhi/rhi_image.h"
#include "rhi/rhi_buffer.h"
#include "rhi/rhi_command_buffer.h"
#include "rhi/rhi_queue.h"
#include <vector>
#include <cstring>

namespace rhi {

bool read_image_to_cpu(
    RHIContext* context,
    RHIImage* image,
    uint32_t width,
    uint32_t height,
    Array<uint8>& out_data,
    bool restore_layout)
{
    const size_t image_size = width * height * 4;
    // 1. Create a host-visible staging buffer
    auto staging_buffer = context->create_buffer(
        image_size,
        RHIBufferUsage::TransferDst,
        RHIMemoryProperty::HostVisible | RHIMemoryProperty::HostCoherent
    );

    // 2. Create and begin a command buffer
    auto cmd = context->create_command_buffer();
    cmd->begin();

    // 2.5 Save old image layout in case we need to restore it
    RHIImageLayout old_layout = image->last_known_layout;

    // 3. Transition image layout for copy
    cmd->transition_image_layout(image, RHIImageLayout::TransferSrc);

    // 4. Copy image to buffer
    cmd->copy_image_to_buffer(image, staging_buffer.get(), width, height);

    // 4.5 Optionally Restore original layout
    if (restore_layout) {
        cmd->transition_image_layout(image, old_layout);
    }

    // 5. End and submit command buffer, wait for completion
    cmd->end();
    context->get_graphics_queue()->submit_and_wait(cmd.get());

    // 6. Map buffer and read data
    out_data.resize(image_size);
    void* data = staging_buffer->map();
    std::memcpy(out_data.data(), data, image_size);
    staging_buffer->unmap();

    return true;
}

} // namespace rhi
