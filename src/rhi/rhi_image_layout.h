#pragma once

namespace rhi {

enum class ImageLayout {
    Undefined,
    General,
    TransferSrc,
    TransferDst,
    ColorAttachment,
    Present,
    // Add more as needed
};

} // namespace rhi
