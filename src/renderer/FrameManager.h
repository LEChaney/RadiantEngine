// FrameManager.h
#pragma once

#include "rhi/interface/command/RHICommandBuffer.h"
#include "rhi/interface/sync/RHISemaphore.h"
#include "rhi/interface/sync/RHIFence.h"
#include "core/CoreDefs.h"

namespace rhi {
class RHIContext;
class RHISwapchain;
class RHIImage;
class RHIImageView;
class RHICommandBuffer;
class RHIFence;
}

namespace renderer {

// --- Swapchain-scoped (indexed by imageIndex) ---
struct SwapchainImageResources {
    uint32              imageIndex;
    rhi::RHIImage*      color;
    rhi::RHIImageView*  colorView;
};

// --- Per-Frame-In-Flight (indexed by currentFrame) ---
struct FrameContext {
    uint32 frameIndex;

    // Commands
    rhi::RHICommandBuffer* cmd;

    // Swapchain
    SwapchainImageResources swapImgs;

    // CPU<->GPU pacing
    rhi::RHISemaphore* imgAvailableSemaphore;   // acquire semaphore
    rhi::RHISemaphore* renderFinishedSemaphore; // present wait semaphore
    rhi::RHIFence* renderFinishedFence;         // signaled when this frame's GPU work is done
};

struct FrameSubmitInfo {
    rhi::RHIPipelineStageFlags waitStageFlags = rhi::RHIPipelineStage::AllCommands;
    rhi::RHIPipelineStageFlags signalStageFlags = rhi::RHIPipelineStage::AllCommands;
};

class FrameManager {
public:
    static UniquePtr<FrameManager> createUnique(rhi::RHIContext* ctx, rhi::RHISwapchain* swapchain,
        uint32 maxFramesInFlight);

    ~FrameManager();

    FrameContext acquireFrame();
    void submitAndPresent(const FrameContext& frame) { submitAndPresent(frame, {}); };
    void submitAndPresent(const FrameContext& frame, const FrameSubmitInfo& frameSubmitInfo);

    uint32 maxFramesInFlight() const { return m_maxFramesInFlight; }

private:
    FrameManager(rhi::RHIContext* ctx, rhi::RHISwapchain* swapchain, uint32 maxFramesInFlight);

    uint32 m_maxFramesInFlight = 3;
    uint32 m_currentFrame = 0; // 0...maxFramesInFlight-1
    rhi::RHIContext* m_ctx = nullptr;
    rhi::RHISwapchain* m_swapchain = nullptr;

    static constexpr uint32 k_expectedFrameCount = 3;
    SmallArray<UniquePtr<rhi::RHICommandBuffer>, k_expectedFrameCount> m_commandBuffers;       // Per-frame-in-flight
    SmallArray<UniquePtr<rhi::RHISemaphore>, k_expectedFrameCount> m_imgAvailableSemaphores;   // Per-frame-in-flight
    SmallArray<UniquePtr<rhi::RHISemaphore>, k_expectedFrameCount> m_renderFinishedSemaphores; // Per-frame-in-flight
    SmallArray<UniquePtr<rhi::RHIFence>, k_expectedFrameCount> m_renderFinishedFences;         // Per-frame-in-flight
};

} // namespace renderer
