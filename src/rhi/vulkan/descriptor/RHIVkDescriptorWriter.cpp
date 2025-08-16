#include "RHIVkDescriptorWriter.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"
#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/buffer/RHIVkBuffer.h"
#include "rhi/interface/descriptor/RHIDescriptorSet.h"
#include "rhi/interface/descriptor/RHIDescriptorWriter.h"

namespace RHI::Vulkan {

// Internal helpers operating on value-type writer
namespace {
    inline RHIVkContext* getCtx(const RHIDescriptorWriter& w) {
        return static_cast<RHIVkContext*>(w.getContext());
    }
    inline RHIVkDescriptorSetLayout* getLayout(const RHIDescriptorWriter& w) {
        return static_cast<RHIVkDescriptorSetLayout*>(w.getData().layout);
    }
    inline uint8* basePtr(const RHIDescriptorWriter& w) {
        return static_cast<uint8*>(w.getData().getMapped());
    }
    inline size_t getBindingOffset(const RHIDescriptorWriter& w, uint32 binding) {
        VkDeviceSize offset = 0;
        auto* vkLayout = getLayout(w);
        vkGetDescriptorSetLayoutBindingOffsetEXT(getCtx(w)->getVkDevice(), vkLayout->getVk(), binding, &offset);
        return static_cast<size_t>(offset);
    }
    inline size_t getStrideForDescType(RHIVkContext* ctx, VkDescriptorType type) {
        const auto& props = ctx->getVkDescriptorBufferProperties();
        switch (type) {
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return props.sampledImageDescriptorSize;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return props.storageImageDescriptorSize;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return props.storageBufferDescriptorSize;
            default: return 0; // expand as needed
        }
    }

    RHIDescriptorWriter& vkWriteSampledImage(RHIDescriptorWriter& self, uint32 binding, uint32 arrayElement, RHIImageView* view, RHISampler* /*sampler*/) {
        RHIVkContext* ctx = getCtx(self);
        auto* rhiVkView = static_cast<RHIVkImageView*>(view);
        ASSERT(rhiVkView && "Null image view provided to vkWriteSampledImage");
        ASSERT(rhiVkView->hasUsage(RHIImageUsage::Sampled) && "Image view must have sampled usage");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = rhiVkView->getVk();
        imageInfo.sampler = VK_NULL_HANDLE;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorGetInfoEXT getInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
        getInfo.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        getInfo.data.pSampledImage = &imageInfo;
        size_t stride = getStrideForDescType(ctx, getInfo.type);
        uint8* dst = basePtr(self) + getBindingOffset(self, binding) + (arrayElement * stride);

        ASSERT(dst && "vkWriteSampledImage: base pointer is null");
        ASSERT(vkGetDescriptorEXT && "vkGetDescriptorEXT not loaded! Did you call volkLoadDevice?");
        vkGetDescriptorEXT(ctx->getVkDevice(), &getInfo, stride, dst);
        return self;
    }

    RHIDescriptorWriter& vkWriteStorageImage(RHIDescriptorWriter& self, uint32 binding, uint32 arrayElement, RHIImageView* view) {
        RHIVkContext* ctx = getCtx(self);
        auto* rhiVkView = static_cast<RHIVkImageView*>(view);
        ASSERT(rhiVkView && "Null image view provided to vkWriteStorageImage");
        ASSERT(rhiVkView->hasUsage(RHIImageUsage::Storage) && "Image view must have storage usage");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = rhiVkView->getVk();
        imageInfo.sampler = VK_NULL_HANDLE;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorGetInfoEXT getInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
        getInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        getInfo.data.pStorageImage = &imageInfo;
        size_t stride = getStrideForDescType(ctx, getInfo.type);
        uint8* dst = basePtr(self) + getBindingOffset(self, binding) + (arrayElement * stride);
        ASSERT(dst && "vkWriteStorageImage: base pointer is null");
        vkGetDescriptorEXT(ctx->getVkDevice(), &getInfo, stride, dst);
        return self;
    }

    RHIDescriptorWriter& vkWriteStorageBuffer(RHIDescriptorWriter& self, uint32 binding,
        uint32 arrayElement, const RHIBufferSlice& bufferSlice) {
        RHIVkContext* ctx = getCtx(self);
        auto* rhiVkBuf = static_cast<RHIVkBuffer*>(bufferSlice.buffer);
        ASSERT(rhiVkBuf && "Null buffer provided to vkWriteStorageBuffer");
        ASSERT(rhiVkBuf->hasUsage(RHIBufferUsage::StorageBuffer) && "Buffer must have storage usage");

        VkDescriptorAddressInfoEXT addrInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
        addrInfo.address = bufferSlice.getDeviceAddress();
        addrInfo.range = bufferSlice.size;
        VkDescriptorGetInfoEXT getInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
        getInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        getInfo.data.pStorageBuffer = &addrInfo;

        size_t stride = getStrideForDescType(ctx, getInfo.type);
        uint8* dst = basePtr(self) + getBindingOffset(self, binding) + (arrayElement * stride);
        ASSERT(dst && "vkWriteStorageBuffer: base pointer is null");
        vkGetDescriptorEXT(ctx->getVkDevice(), &getInfo, stride, dst);
        return self;
    }

    void vkFlush(RHIDescriptorWriter& /*self*/) {
        // If descriptor buffer is non-coherent, flush here. For now, assume coherent; no-op.
    }

    constexpr RHIDescriptorWriterOps gk_vkWriterOps{
        &vkWriteSampledImage,
        &vkWriteStorageImage,
        &vkWriteStorageBuffer,
        &vkFlush
    };
} // anonymous namespace

const RHIDescriptorWriterOps* getVkDescriptorWriterOps() {
    return &gk_vkWriterOps;
}

} // namespace rhi::vulkan