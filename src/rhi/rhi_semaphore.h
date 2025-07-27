#pragma once

namespace rhi {

class RHISemaphore {
public:
    virtual ~RHISemaphore() = default;

protected:
    // Only derived context or implementation should create RHISemaphore objects
    RHISemaphore() = default;
    RHISemaphore(const RHISemaphore&) = delete;
    RHISemaphore& operator=(const RHISemaphore&) = delete;
    RHISemaphore(RHISemaphore&&) = delete;
    RHISemaphore& operator=(RHISemaphore&&) = delete;
};

} // namespace rhi
