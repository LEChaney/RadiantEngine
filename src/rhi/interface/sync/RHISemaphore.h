#pragma once

namespace rhi {

class RHISemaphore {
public:
    virtual ~RHISemaphore() = default;

    RHISemaphore(const RHISemaphore&) = delete;
    RHISemaphore& operator=(const RHISemaphore&) = delete;
    RHISemaphore(RHISemaphore&&) = delete;
    RHISemaphore& operator=(RHISemaphore&&) = delete;

protected:
    RHISemaphore() = default;
};

} // namespace rhi
