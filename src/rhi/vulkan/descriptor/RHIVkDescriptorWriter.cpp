#include "RHIVkDescriptorWriter.h"
#include "rhi/vulkan/core/RHIVkContext.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.h"
#include "rhi/vulkan/descriptor/RHIVkDescriptorBufferArena.h"
#include "rhi/vulkan/image/RHIVkImageView.h"
#include "rhi/vulkan/buffer/RHIVkBuffer.h"

namespace rhi::vulkan {

UniquePtr<RHIVkDescriptorWriter> RHIVkDescriptorWriter::createUnique(RHIVkContext* ctx, const RHIDescriptorSetAllocation& alloc) {
    return UniquePtr<RHIVkDescriptorWriter>(new RHIVkDescriptorWriter(ctx, alloc));
}

RHIVkDescriptorWriter::RHIVkDescriptorWriter(RHIVkContext* ctx, const RHIDescriptorSetAllocation& alloc)
    : RHIDescriptorWriter(alloc), m_ctx(ctx) {}

VkDescriptorSetLayout RHIVkDescriptorWriter::getVkLayout() const {
    return static_cast<RHIVkDescriptorSetLayout*>(m_alloc.layout)->getVk();
}

VkDevice RHIVkDescriptorWriter::getDevice() const { return m_ctx->getVkDevice(); }

uint8* RHIVkDescriptorWriter::basePtr() const {
    return static_cast<uint8*>(m_alloc.arena->mapped()) + m_alloc.offset;
}

size_t RHIVkDescriptorWriter::bindingOffset(uint32 binding) const {
    VkDeviceSize offset = 0;
    vkGetDescriptorSetLayoutBindingOffsetEXT(getDevice(), getVkLayout(), binding, &offset);
    return static_cast<size_t>(offset);
}

size_t RHIVkDescriptorWriter::descriptorStrideForType(VkDescriptorType type) {
    const auto& props = m_ctx->getVkDescriptorBufferProperties();
    switch (type) {
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return props.sampledImageDescriptorSize;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return props.storageImageDescriptorSize;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return props.storageBufferDescriptorSize;
        default: return 0; // expand as needed
    }
}

void RHIVkDescriptorWriter::writeDescriptorRecord(VkDescriptorGetInfoEXT& getInfo, void* dst, size_t dataSize) {
    ASSERT(m_alloc.arena->isValidRange(dst, dataSize) && 
        "RHIVkDescriptorWriter::writeDescriptorRecord: Invalid destination range");

    ASSERT(vkGetDescriptorEXT && "vkGetDescriptorEXT not loaded! Did you call volkLoadDevice?");
    vkGetDescriptorEXT(getDevice(), &getInfo, dataSize, dst);
}

RHIDescriptorWriter& RHIVkDescriptorWriter::writeSampledImage(uint32 binding, uint32 arrayElement, RHIImageView* view, RHISampler* /*sampler*/) {
    RHIVkImageView* rhiVkView = static_cast<RHIVkImageView*>(view);
    ASSERT(rhiVkView && "Null image view provided to RHIVkDescriptorWriter::writeSampledImage");
    ASSERT(rhiVkView->hasUsage(RHIImageUsage::Sampled) && 
        "RHIVkDescriptorWriter::writeSampledImage: Image view must have sampled usage");
    
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = view ? static_cast<RHIVkImageView*>(view)->getVk() : VK_NULL_HANDLE;
    imageInfo.sampler = VK_NULL_HANDLE;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorGetInfoEXT getInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
    getInfo.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    getInfo.data.pSampledImage = &imageInfo;
    size_t stride = descriptorStrideForType(getInfo.type);
    uint8* dst = basePtr() + bindingOffset(binding) + (arrayElement * stride);
    writeDescriptorRecord(getInfo, dst, stride);
    return *this;
}

RHIDescriptorWriter& RHIVkDescriptorWriter::writeStorageImage(uint32 binding, uint32 arrayElement, RHIImageView* view) {
    auto* rhiVkView = static_cast<RHIVkImageView*>(view);
    ASSERT(rhiVkView && "Null image view provided to RHIVkDescriptorWriter::writeStorageImage");
    ASSERT(rhiVkView->hasUsage(RHIImageUsage::Storage) && 
        "RHIVkDescriptorWriter::writeStorageImage: Image view must have storage usage");
    
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = rhiVkView->getVk();
    imageInfo.sampler = VK_NULL_HANDLE;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorGetInfoEXT getInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
    getInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    getInfo.data.pStorageImage = &imageInfo;
    size_t stride = descriptorStrideForType(getInfo.type);
    uint8* dst = basePtr() + bindingOffset(binding) + (arrayElement * stride);
    writeDescriptorRecord(getInfo, dst, stride);
    return *this;
}

RHIDescriptorWriter& RHIVkDescriptorWriter::writeStorageBuffer(uint32 binding, uint32 arrayElement, RHIBuffer* buffer, size_t offset, size_t range) {
    VkDescriptorAddressInfoEXT addrInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
    auto* rhiVkBuf = static_cast<RHIVkBuffer*>(buffer);
    ASSERT(rhiVkBuf && "Null buffer provided to RHIVkDescriptorWriter::writeStorageBuffer");
    ASSERT(rhiVkBuf->hasUsage(RHIBufferUsage::StorageBuffer) && 
        "RHIVkDescriptorWriter::writeStorageBuffer: Buffer must have storage usage");

    VkBufferDeviceAddressInfo addrInfoQuery{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfoQuery.buffer = rhiVkBuf->getVk();
    addrInfo.address = vkGetBufferDeviceAddress(getDevice(), &addrInfoQuery) + offset;
    addrInfo.range = range;
    
    VkDescriptorGetInfoEXT getInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
    getInfo.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    getInfo.data.pStorageBuffer = &addrInfo;
    size_t stride = descriptorStrideForType(getInfo.type);
    uint8* dst = basePtr() + bindingOffset(binding) + (arrayElement * stride);
    writeDescriptorRecord(getInfo, dst, stride);
    return *this;
}

} // namespace rhi::vulkan