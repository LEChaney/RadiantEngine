// FrameManager.h
#pragma once

#include "rhi/interface/command/RHICommandBuffer.h"
#include "rhi/interface/sync/RHISemaphore.h"
#include "rhi/interface/sync/RHIFence.h"
#include "core/CoreDefs.h"

namespace RHI {
class RHIContext;
class RHISwapchain;
class RHIImage;
class RHIImageView;
class RHIQueue;
}

namespace Renderer {

// --- Swapchain-scoped (indexed by imageIndex) ---
struct SwapchainImageResources {
    uint32              imageIndex;
    RHI::RHIImage*      color;
    RHI::RHIImageView*  colorView;
};

// --- Per-Frame-In-Flight (indexed by currentFrame) ---
struct FrameContext {
    uint32 frameIndex;

    // Commands
    RHI::RHICommandBuffer* cmd;

    // Swapchain
    SwapchainImageResources swapImgs;

    // Synchronization
    RHI::RHISemaphore* imgAvailableSemaphore;   // acquire semaphore
    RHI::RHISemaphore* renderFinishedSemaphore; // present wait semaphore
    RHI::RHIFence* renderFinishedFence;         // signaled when this frame's GPU work is done
};

struct FrameSubmitInfo {
    RHI::RHIQueue* queue = nullptr;
    RHI::RHIPipelineStageFlags waitAcquireStage = RHI::RHIPipelineStage::AllCommands;
    RHI::RHIPipelineStageFlags signalPresentStage = RHI::RHIPipelineStage::AllCommands;
};

class FrameManager {
public:
    static UniquePtr<FrameManager> createUnique(RHI::RHIContext* ctx, RHI::RHISwapchain* swapchain,
        uint32 maxFramesInFlight);

    ~FrameManager();

    FrameManager(const FrameManager&) = delete;
    FrameManager& operator=(const FrameManager&) = delete;
    FrameManager(FrameManager&&) = delete;
    FrameManager& operator=(FrameManager&&) = delete;

    FrameContext acquireFrame();
    void submitAndPresent(const FrameContext& frame) { submitAndPresent(frame, {}); };
    void submitAndPresent(const FrameContext& frame, const FrameSubmitInfo& frameSubmitInfo);

    uint32 maxFramesInFlight() const { return m_maxFramesInFlight; }

private:
    FrameManager(RHI::RHIContext* ctx, RHI::RHISwapchain* swapchain, uint32 maxFramesInFlight);

    uint32 m_maxFramesInFlight = 3;
    uint32 m_currentFrame = 0; // 0...maxFramesInFlight-1
    RHI::RHIContext* m_ctx = nullptr;
    RHI::RHISwapchain* m_swapchain = nullptr;

    static constexpr uint32 k_expectedFrameCount = 3;
    SmallArray<UniquePtr<RHI::RHICommandBuffer>, k_expectedFrameCount> m_commandBuffers;       // Per-frame-in-flight
    SmallArray<UniquePtr<RHI::RHISemaphore>, k_expectedFrameCount> m_imgAvailableSemaphores;   // Per-frame-in-flight
    SmallArray<UniquePtr<RHI::RHISemaphore>, k_expectedFrameCount> m_renderFinishedSemaphores; // Per-frame-in-flight
    SmallArray<UniquePtr<RHI::RHIFence>, k_expectedFrameCount> m_renderFinishedFences;         // Per-frame-in-flight
};

} // namespace renderer
