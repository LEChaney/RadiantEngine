#pragma once

#include <vk_types.h>

struct DescriptorLayoutBuilder {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    
    void add_binding(uint32 binding, VkDescriptorType type);
    void clear();

    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags stageFlags, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAlllocator {

    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    VkDescriptorPool pool;

    void init_pool(VkDevice device, uint32 maxSets, std::span<PoolSizeRatio> poolRatios);
    void destroy_pool(VkDevice device);
    void clear_descriptors(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};

struct DescriptorAllocatorGrowable {
public:
    struct PoolSizeRatio {
        VkDescriptorType type;
        /* 
        * This is something like a guess as to how many descriptors of this type
        * will be allocated per set. This is used to create the pool sizes which
        * set a hard limit on the number of descriptors of this type that can be
        * allocated from this pool across ALL sets.
        **/
        float ratio;
    };

    void init(VkDevice device, uint32 initialSets, std::span<PoolSizeRatio> poolRatios);
    void reset_pools(VkDevice device);
    void destroy_pools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);

private:
    VkDescriptorPool get_pool(VkDevice device);
    VkDescriptorPool create_pool(VkDevice device, uint32 setCount, std::span<PoolSizeRatio> poolRatios);

    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> fullPools;
    std::vector<VkDescriptorPool> readyPools;
    uint32 setsPerPool;
};

struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void write_image(uint32 binding, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout, VkDescriptorType type);
    void write_buffer(uint32 binding, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset, VkDescriptorType type);

    void clear();
    void update_set(VkDevice device, VkDescriptorSet set);
};
