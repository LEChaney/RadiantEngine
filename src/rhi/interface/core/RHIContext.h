#pragma once
#include "rhi/interface/core/RHICoreDefs.h"
#include "core/CoreDefs.h"

class SDL_Window;

namespace RHI {

class RHIQueue;
class RHICommandBuffer;
class RHIFence;
class RHISemaphore;
class RHISwapchain;
class RHIBuffer;
class RHIImage;
class RHIShaderModule;
class RHIPipeline;
class RHIDescriptorSet;
class RHIDescriptorSetLayoutBuilder;
class RHIDescriptorBuffer;
class RHIDescriptorBufferCreateInfo;
class RHIPipelineLayoutBuilder;
class RHIComputePipelineDescriptor;
class RHIDescriptorWriter;

// ops for value-type builders
struct RHIDescriptorWriterOps;
struct RHIDescriptorSetLayoutBuilderOps;
struct RHIPipelineLayoutBuilderOps;

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
    virtual UniquePtr<RHISwapchain> createSwapchain(SDL_Window* window, uint32 width, uint32 height, uint32 bufferCount, RHIImageUsageFlags extraImageUsage = 0) = 0;
    virtual UniquePtr<RHIBuffer> createBuffer(uint64 size, RHIBufferUsageFlags usage, RHIMemoryPropertyFlags memProps) = 0;
    virtual UniquePtr<RHIImage> createImage(uint32 width, uint32 height, RHIFormat format, RHIImageUsageFlags usage, RHIMemoryPropertyFlags memProps) = 0;
    virtual UniquePtr<RHIShaderModule> createShaderModule(const Path& spvFilePath) = 0;
    virtual UniquePtr<RHIShaderModule> createShaderModule(const Array<uint32>& shaderCode) = 0;
    virtual UniquePtr<RHIPipeline> createComputePipeline(const RHIComputePipelineDescriptor& desc) = 0;
    virtual UniquePtr<RHIDescriptorBuffer> createDescriptorBuffer(const RHIDescriptorBufferCreateInfo& createInfo) = 0;

    // Backend ops table providers for value-types
    virtual const RHIDescriptorWriterOps* getDescriptorWriterOps() const = 0;
    virtual const RHIDescriptorSetLayoutBuilderOps* getDescriptorSetLayoutBuilderOps() const = 0;
    virtual const RHIPipelineLayoutBuilderOps* getPipelineLayoutBuilderOps() const = 0;

protected:
    // Only derived context or implementation should create RHIContext objects
    RHIContext() = default;
};

} // namespace rhi
