#pragma once
#include "rhi/interface/image/RHIImage.h" // new include for usage queries

namespace rhi {

class RHIImage;

class RHIImageView {
public:
    virtual ~RHIImageView() = default;

    virtual RHIImage* getImage() const = 0;

    // New convenience helpers
    RHIImageUsageFlags getUsage() const { return getImage() ? getImage()->getUsage() : 0; }
    bool hasUsage(RHIImageUsageFlags usage) const { return getImage() && getImage()->hasUsage(usage); }

protected:
    // Only derived context or implementation should create RHIImageView objects
    RHIImageView() = default;
    RHIImageView(const RHIImageView&) = delete;
    RHIImageView& operator=(const RHIImageView&) = delete;
    RHIImageView(RHIImageView&&) = delete;
    RHIImageView& operator=(RHIImageView&&) = delete;
};

} // namespace rhi
