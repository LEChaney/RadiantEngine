#pragma once

namespace rhi {

class RHIImage {
public:
    RHIImage() = default;
    virtual ~RHIImage() = default;

    RHIImage(const RHIImage&) = delete;
    RHIImage& operator=(const RHIImage&) = delete;
    RHIImage(RHIImage&&) = delete;
    RHIImage& operator=(RHIImage&&) = delete;
};

} // namespace rhi
