#pragma once
#include <vector>

namespace rhi {

class RHICommandBuffer;
class RHIFence;
class RHISemaphore;

class RHIQueue {
public:
    virtual void submit(const std::vector<RHICommandBuffer*>& commandBuffers, RHIFence* fence, RHISemaphore* waitSemaphore) = 0;
    virtual void wait_idle() = 0;
    virtual ~RHIQueue() = default;
};

} // namespace rhi
