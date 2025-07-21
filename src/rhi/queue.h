#pragma once
#include <vector>

namespace rhi {

class CommandBuffer;
class Fence;
class Semaphore;

class Queue {
public:
    virtual void submit(const std::vector<CommandBuffer*>& commandBuffers, Fence* fence, Semaphore* waitSemaphore) = 0;
    virtual void wait_idle() = 0;
    virtual ~Queue() = default;
};

} // namespace rhi
