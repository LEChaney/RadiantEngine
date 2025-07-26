#pragma once

namespace rhi {

class RHIImage;
class RHIImageView {
public:
    RHIImageView() = default;
    virtual ~RHIImageView() = default;

    virtual RHIImage* get_image() const = 0;

    RHIImageView(const RHIImageView&) = delete;
    RHIImageView& operator=(const RHIImageView&) = delete;
    RHIImageView(RHIImageView&&) = delete;
    RHIImageView& operator=(RHIImageView&&) = delete;
};

} // namespace rhi
