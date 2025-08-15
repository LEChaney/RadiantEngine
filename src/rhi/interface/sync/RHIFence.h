#pragma once

namespace RHI {

class RHIFence {
public:
    virtual ~RHIFence() = default;
    
    virtual void wait() = 0; // Wait for the fence to signal
    virtual void reset() = 0; // Reset the fence state
    virtual bool isSignaled() const = 0; // Check if the fence is signaled

protected:
    // Only derived context or implementation should create RHIFence objects
    RHIFence() = default;
    RHIFence(const RHIFence&) = delete;
    RHIFence& operator=(const RHIFence&) = delete;
    RHIFence(RHIFence&&) = delete;
    RHIFence& operator=(RHIFence&&) = delete;
};

} // namespace rhi
