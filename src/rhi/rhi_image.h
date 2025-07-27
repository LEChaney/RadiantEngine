#pragma once
#include "rhi_image_layout.h"

namespace rhi {

class RHIImage {
public:
    RHIImage() = default;
    virtual ~RHIImage() = default;

    RHIImage(const RHIImage&) = delete;
    RHIImage& operator=(const RHIImage&) = delete;
    RHIImage(RHIImage&&) = delete;
    RHIImage& operator=(RHIImage&&) = delete;

    // Track last submitted layout for image, useful for tracking
    // the current layout at the start of command buffer recording.
    ImageLayout last_known_layout = ImageLayout::Undefined;
};

} // namespace rhi
