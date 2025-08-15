#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"

namespace RHI {

class RHICommandBuffer;
class RHIFence;
class RHISemaphore;

// Extended semaphore submit info mirroring VkSemaphoreSubmitInfo for sync2 style submissions.
struct RHISemaphoreSubmitInfo {
    RHISemaphore* semaphore = nullptr;            // Binary or timeline (timeline not yet implemented)
    RHIPipelineStageFlags stageMask = RHIPipelineStage::AllCommands; // For wait: stages that will wait. For signal: producing stage (single bit recommended)
    uint64 value = 0;                             // Timeline value (0 for binary)
};

struct RHIQueueSubmitDesc {
    SmallArray<RHICommandBuffer*, 1> commandBuffers;      // Command buffers to submit
    SmallArray<RHISemaphoreSubmitInfo, 1> waits;          // Wait semaphores (can be empty)
    SmallArray<RHISemaphoreSubmitInfo, 1> signals;        // Signal semaphores (can be empty)
    RHIFence* fence = nullptr;                            // Optional fence to signal
};

class RHIQueue {
public:
    virtual ~RHIQueue() = default;

    void submit(const SmallArray<RHICommandBuffer*, 1>& commandBuffers) {
        submit(commandBuffers, nullptr, nullptr, nullptr);
    }
    virtual void submit(const SmallArray<RHICommandBuffer*, 1>& commandBuffers,
        RHISemaphore* waitSemaphore, RHISemaphore* signalSemaphore, RHIFence* fence) = 0;
    // Extended submit supporting multiple semaphores & stage masks.
    virtual void submit(const RHIQueueSubmitDesc& desc) {
        // Fallback: map to legacy API using the first wait/signal only.
        RHISemaphore* firstWait = desc.waits.empty() ? nullptr : desc.waits.front().semaphore;
        RHISemaphore* firstSignal = desc.signals.empty() ? nullptr : desc.signals.front().semaphore;
        submit(desc.commandBuffers, firstWait, firstSignal, desc.fence);
    }
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
