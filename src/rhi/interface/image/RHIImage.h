#pragma once
#include "rhi/interface/core/RHICoreDefs.h"

namespace RHI {

class RHIImage {
public:  
    virtual ~RHIImage() = default;

    uint32 getWidth() const { return m_width; }
    uint32 getHeight() const { return m_height; }
    RHIFormat getFormat() const { return m_format; }
    RHIImageUsageFlags getUsage() const { return m_usage; }
    bool hasUsage(RHIImageUsageFlags usage) const { return (m_usage & usage) == usage; }

    // Track last submitted layout for image, useful for tracking
    // the current layout at the start of command buffer recording.
    RHIImageLayout lastLayout = RHIImageLayout::Undefined;

protected:
    // Only derived context or implementation should create RHIImage objects
    RHIImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage = 0)
        : m_width(width), m_height(height), m_format(format), m_usage(usage) {};
    RHIImage(const RHIImage&) = delete;
    RHIImage& operator=(const RHIImage&) = delete;
    RHIImage(RHIImage&&) = delete;
    RHIImage& operator=(RHIImage&&) = delete;

    uint32 m_width = 0;
    uint32 m_height = 0;
    RHIFormat m_format = RHIFormat::RHI_FORMAT_UNDEFINED;
    RHIImageUsageFlags m_usage = 0;
};

} // namespace rhi
