#pragma once
#include "core/core_defs.h"
#include <vector>

namespace rhi {

class RHICommandBuffer;
class RHIFence;
class RHISemaphore;

class RHIQueue {
public:
    RHIQueue() = default;
    virtual ~RHIQueue() = default;

    RHIQueue(const RHIQueue&) = delete;
    RHIQueue& operator=(const RHIQueue&) = delete;
    RHIQueue(RHIQueue&&) = delete;
    RHIQueue& operator=(RHIQueue&&) = delete;

    virtual void submit(const Array<RHICommandBuffer*>& commandBuffers, RHIFence* fence, RHISemaphore* waitSemaphore) = 0;
    virtual void wait_idle() = 0;
};

} // namespace rhi
