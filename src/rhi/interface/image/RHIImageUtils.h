#pragma once
#include "core/CoreDefs.h"
#include <vector>

namespace RHI {
class RHIImage;
class RHIContext;

// Reads back the contents of an RHIImage into a CPU buffer (RGBA8)
// Returns true on success, false on failure.
// TODO: Add format and other parameters as needed
// This function assumes the image is in a format compatible with RGBA8 readback.
bool readImageToCpu(
    RHIContext* context, 
    RHIImage* image, 
    uint32 width, 
    uint32 height, 
    Array<uint8>& outData,
    bool restoreLayout = false
);

} // namespace rhi
