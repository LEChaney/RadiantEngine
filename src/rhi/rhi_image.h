#pragma once
#include "rhi_core_defs.h"

namespace rhi {

class RHIImage {
public:  
    virtual ~RHIImage() = default;

    uint32 get_width() const { return m_width; }
    uint32 get_height() const { return m_height; }
    RHIFormat get_format() const { return m_format; }

    // Track last submitted layout for image, useful for tracking
    // the current layout at the start of command buffer recording.
    RHIImageLayout last_known_layout = RHIImageLayout::Undefined;

protected:
    // Only derived context or implementation should create RHIImage objects
    RHIImage(uint32 width, uint32 height, RHIFormat format)
        : m_width(width), m_height(height), m_format(format) {};
    RHIImage(const RHIImage&) = delete;
    RHIImage& operator=(const RHIImage&) = delete;
    RHIImage(RHIImage&&) = delete;
    RHIImage& operator=(RHIImage&&) = delete;

    uint32 m_width = 0;
    uint32 m_height = 0;
    RHIFormat m_format = RHIFormat::RHI_FORMAT_UNDEFINED;
};

} // namespace rhi
