#pragma once
#include <cstdint>

namespace rhi {

enum class BufferUsage : uint8_t {
    TransferSrc = 1,
    TransferDst = 1 << 1,
    Uniform     = 1 << 2,
    Vertex      = 1 << 3,
    Index       = 1 << 4,
    // ... add more as needed (up to 8)
};

enum class MemoryProperty : uint8_t {
    DeviceLocal     = 1,
    HostVisible     = 1 << 1,
    HostCoherent    = 1 << 2,
    // ... add more as needed (up to 8)
};

// For BufferUsage
inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline BufferUsage operator&(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

// For MemoryProperty (or MemoryFlags)
inline MemoryProperty operator|(MemoryProperty a, MemoryProperty b) {
    return static_cast<MemoryProperty>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline MemoryProperty operator&(MemoryProperty a, MemoryProperty b) {
    return static_cast<MemoryProperty>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

} // namespace rhi
