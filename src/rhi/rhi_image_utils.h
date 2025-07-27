#pragma once
#include <vector>
#include <cstdint>

namespace rhi {
class RHIImage;
class RHIContext;

// Reads back the contents of an RHIImage into a CPU buffer (RGBA8)
// Returns true on success, false on failure.
// TODO: Add format and other parameters as needed
// This function assumes the image is in a format compatible with RGBA8 readback.
bool read_image_to_cpu(RHIContext* context, RHIImage* image, uint32_t width, uint32_t height, std::vector<uint8_t>& outData);

} // namespace rhi
