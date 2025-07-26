#pragma once

namespace rhi {

class RHIFence {
public:
    RHIFence() = default;
    virtual ~RHIFence() = default;

    RHIFence(const RHIFence&) = delete;
    RHIFence& operator=(const RHIFence&) = delete;
    RHIFence(RHIFence&&) = delete;
    RHIFence& operator=(RHIFence&&) = delete;
};

} // namespace rhi
