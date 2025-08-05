#pragma once

namespace rhi {

class RHIImage;

class RHIImageView {
public:
    virtual ~RHIImageView() = default;

    virtual RHIImage* getImage() const = 0;

protected:
    // Only derived context or implementation should create RHIImageView objects
    RHIImageView() = default;
    RHIImageView(const RHIImageView&) = delete;
    RHIImageView& operator=(const RHIImageView&) = delete;
    RHIImageView(RHIImageView&&) = delete;
    RHIImageView& operator=(RHIImageView&&) = delete;
};

} // namespace rhi
