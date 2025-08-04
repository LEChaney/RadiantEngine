#pragma once
#include "core/core_defs.h"
#include "rhi_core_defs.h"
#include <string>

class SDL_Window;

namespace rhi {

class RHIQueue;
class RHICommandBuffer;
class RHIFence;
class RHISemaphore;
class RHISwapchain;
class RHIBuffer;
class RHIImage;
class RHIShaderModule;
class RHIPipeline;
class RHIDescriptorSetLayoutBuilder;
class RHIDescriptorSetBuilder;

class RHIContext {
public:
    virtual ~RHIContext() = default;

    RHIContext(const RHIContext&) = delete;
    RHIContext& operator=(const RHIContext&) = delete;
    RHIContext(RHIContext&&) = delete;
    RHIContext& operator=(RHIContext&&) = delete;

    virtual RHIQueue* getGraphicsQueue() = 0;

    // Factory methods for creating RHI objects
    virtual UniquePtr<RHICommandBuffer> createCommandBuffer() = 0;
    virtual UniquePtr<RHIFence> createFence() = 0;
    virtual UniquePtr<RHISemaphore> createSemaphore() = 0;
    virtual UniquePtr<RHISwapchain> createSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 bufferCount) = 0;
    virtual UniquePtr<RHIBuffer> createBuffer(uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) = 0;
    virtual UniquePtr<RHIImage> createImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage, RHIMemoryPropertyFlags memProps) = 0;
    virtual UniquePtr<RHIShaderModule> createShaderModule(const std::string& spvFilePath) = 0;
    virtual UniquePtr<RHIPipeline> createComputePipeline(RHIShaderModule* shaderModule) = 0;
    virtual UniquePtr<RHIDescriptorSetLayoutBuilder> createDescriptorSetLayoutBuilder() = 0;
    virtual UniquePtr<RHIDescriptorSetBuilder> createDescriptorSetBuilder() = 0;

protected:
    // Only derived context or implementation should create RHIContext objects
    RHIContext() = default;
};

} // namespace rhi
