// FrameManager.cpp
#include "renderer/FrameManager.h"
#include "rhi/interface/swapchain/RHISwapchain.h"
#include "rhi/interface/core/RHIContext.h"
#include "rhi/interface/queue/RHIQueue.h"
#include "core/CoreDefs.h"

using namespace RHI;

namespace Renderer {

UniquePtr<FrameManager> FrameManager::createUnique(RHIContext* ctx, RHISwapchain* swapchain,
    uint32 maxFramesInFlight)
{
    return UniquePtr<FrameManager>(new FrameManager(ctx, swapchain, maxFramesInFlight));
}

FrameManager::FrameManager(RHIContext* ctx, RHISwapchain* swapchain, uint32 maxFramesInFlight)
    : m_ctx(ctx), m_swapchain(swapchain)
{
    ASSERT(m_ctx && m_swapchain);
    m_maxFramesInFlight = maxFramesInFlight;

    m_commandBuffers.resize(m_maxFramesInFlight);
    m_imgAvailableSemaphores.resize(m_maxFramesInFlight);
    m_renderFinishedSemaphores.resize(m_maxFramesInFlight);
    m_renderFinishedFences.resize(m_maxFramesInFlight);

    for (uint32 i = 0; i < m_maxFramesInFlight; ++i) {
        m_commandBuffers[i] = m_ctx->createCommandBuffer();
        m_imgAvailableSemaphores[i] = m_ctx->createSemaphore();
        m_renderFinishedSemaphores[i] = m_ctx->createSemaphore();
        m_renderFinishedFences[i] = m_ctx->createFence();
    }
}
FrameManager::~FrameManager() {
    // Wait for command buffer executions to complete on the queues they were submitted to
    for (auto& fence : m_renderFinishedFences) {
        fence->wait();
    }

    // Wait for the graphics queue to be idle explicitly, to catch any queue present
    // operations that might be using resources.
    m_ctx->getGraphicsQueue()->waitIdle();
}

FrameContext FrameManager::acquireFrame() {
    // Wait for the fence to ensure the frame is ready
    // NOTE: Controls latency / how far ahead the CPU can run.
    //       In this case, we block if we see the frame again before it has finished
    //       rendering (and this CPU fence has been signaled). Max latency here is essentially
    //       the swapchain image count number of frames (in FIFO present mode).
    auto& fence = m_renderFinishedFences[m_currentFrame];
    fence->wait();
    fence->reset();

    auto* imgAvailableSemaphore = m_imgAvailableSemaphores[m_currentFrame].get();

    uint32 imageIndex = m_swapchain->acquireNextImage(imgAvailableSemaphore);
    SwapchainImageResources swapchainImage{
        .imageIndex = imageIndex,
        .color = m_swapchain->getImage(imageIndex),
        .colorView = m_swapchain->getImageView(imageIndex)
    };

    FrameContext fr{
        .frameIndex = m_currentFrame,
        .cmd = m_commandBuffers[m_currentFrame].get(),
        .swapImgs = swapchainImage,
        .imgAvailableSemaphore = m_imgAvailableSemaphores[m_currentFrame].get(),
        .renderFinishedSemaphore = m_renderFinishedSemaphores[m_currentFrame].get(),
        .renderFinishedFence = m_renderFinishedFences[m_currentFrame].get()
    };

    m_commandBuffers[m_currentFrame]->reset();
    m_commandBuffers[m_currentFrame]->begin();

    return fr;
}

void FrameManager::submitAndPresent(const FrameContext& frame, const FrameSubmitInfo& frameSubmitInfo) {
    m_commandBuffers[m_currentFrame]->end();

    // TODO: Add ability to pass in queue to submit to
    auto* queue = frameSubmitInfo.queue ? frameSubmitInfo.queue : m_ctx->getGraphicsQueue();
    RHIQueueSubmitDesc queueSubmitInfo{
        .commandBuffers = {frame.cmd},
        .waits = {{frame.imgAvailableSemaphore, frameSubmitInfo.waitAcquireStage}},
        .signals = {{frame.renderFinishedSemaphore, frameSubmitInfo.signalPresentStage}},
        .fence = frame.renderFinishedFence
    };
    queue->submit(queueSubmitInfo);

    m_swapchain->present(frame.swapImgs.imageIndex, frame.renderFinishedSemaphore);
    m_currentFrame = (m_currentFrame + 1) % m_maxFramesInFlight;
}

} // namespace renderer
