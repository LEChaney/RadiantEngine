#pragma once
#include "core/CoreDefs.h"
#include <vector>


namespace rhi {

class RHICommandBuffer;
class RHIFence;
class RHISemaphore;

class RHIQueue {
public:
    virtual ~RHIQueue() = default;

    virtual void submit(const Array<RHICommandBuffer*>& commandBuffers, RHIFence* fence, RHISemaphore* waitSemaphore) = 0;
    virtual void waitIdle() = 0;

    // Submit a single command buffer and wait for completion (for readback, utility)
    virtual void submitAndWait(RHICommandBuffer* cmd) = 0;

protected:
    // Only derived context or implementation should create RHIQueue objects
    RHIQueue() = default;
    RHIQueue(const RHIQueue&) = delete;
    RHIQueue& operator=(const RHIQueue&) = delete;
    RHIQueue(RHIQueue&&) = delete;
    RHIQueue& operator=(RHIQueue&&) = delete;
};

} // namespace rhi
