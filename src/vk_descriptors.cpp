#include <vk_descriptors.h>

void DescriptorLayoutBuilder::add_binding(uint32 binding, VkDescriptorType type)
{
    VkDescriptorSetLayoutBinding newBinding = {};
    newBinding.binding = binding;
    newBinding.descriptorType = type;
    newBinding.descriptorCount = 1;

    bindings.push_back(newBinding);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags stageFlags, void *pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    for (auto& binding : bindings) {
        binding.stageFlags |= stageFlags;
    }

    VkDescriptorSetLayoutCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.pNext = pNext;

    info.pBindings = bindings.data();
    info.bindingCount = static_cast<uint32>(bindings.size());
    info.flags = flags;

    VkDescriptorSetLayout set;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

    return set;
}

void DescriptorAlllocator::init_pool(VkDevice device, uint32 maxSets, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const PoolSizeRatio& ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = static_cast<uint32>(ratio.ratio * maxSets)
        });
    }

    VkDescriptorPoolCreateInfo poolInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);
}

void DescriptorAlllocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}

void DescriptorAlllocator::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

VkDescriptorSet DescriptorAlllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &set));

    return set;
}

void DescriptorAllocatorGrowable::init(VkDevice device, uint32 initialSets, std::span<PoolSizeRatio> poolRatios)
{
    ratios.clear();
    ratios.reserve(poolRatios.size());
    for (const PoolSizeRatio& r : poolRatios) {
        ratios.push_back(r);
    }
    
    VkDescriptorPool newPool = create_pool(device, initialSets, poolRatios);
    setsPerPool = initialSets * 1.5;
    readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::reset_pools(VkDevice device)
{
    for (VkDescriptorPool pool : readyPools) {
        vkResetDescriptorPool(device, pool, 0);
    }
    for (VkDescriptorPool pool : fullPools) {
        vkResetDescriptorPool(device, pool, 0);
        readyPools.push_back(pool);
    }
    fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device)
{
    for (VkDescriptorPool pool : readyPools) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    for (VkDescriptorPool pool : fullPools) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    readyPools.clear();
    fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void *pNext)
{
    // get of create a pool to allocate from
    VkDescriptorPool poolToUse = get_pool(device);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = pNext;
    allocInfo.descriptorPool = poolToUse;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, &set);

    // allocation failed. Try again
    if (allocResult == VK_ERROR_FRAGMENTED_POOL || allocResult == VK_ERROR_OUT_OF_POOL_MEMORY) {
        // this pool is full, so we need to add it to the full pool list
        fullPools.push_back(poolToUse);
        // and create a new one
        poolToUse = get_pool(device);
        allocInfo.descriptorPool = poolToUse;

        // try again with new pool. We fail if the second pool fails to allocate the set
        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &set));
    }

    readyPools.push_back(poolToUse);
    return set;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device)
{
    VkDescriptorPool newPool;

    if (readyPools.size() > 0) {
        newPool = readyPools.back();
        readyPools.pop_back();
    } else {
        // need to create a new pool
        newPool = create_pool(device, setsPerPool, ratios);

        setsPerPool = setsPerPool * 1.5;
        if (setsPerPool > 4092) {
            setsPerPool = 4092;
        }
    }

    return newPool;
}

VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32 setCount, std::span<PoolSizeRatio> poolRatios)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const PoolSizeRatio& ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = static_cast<uint32>(ratio.ratio * setCount)
        });
    }

    VkDescriptorPoolCreateInfo poolInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.pNext = nullptr;
    poolInfo.flags = 0;
    poolInfo.maxSets = setCount;
    poolInfo.poolSizeCount = static_cast<uint32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool);

    return newPool;
}

void DescriptorWriter::write_image(uint32 binding, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout, VkDescriptorType type)
{
    VkDescriptorImageInfo& imageInfo = imageInfos.emplace_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = imageView,
        .imageLayout = imageLayout
    });

    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &imageInfo;

    writes.push_back(write);
}

void DescriptorWriter::write_buffer(uint32 binding, VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset, VkDescriptorType type)
{
    VkDescriptorBufferInfo& bufferInfo = bufferInfos.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = size
    });

    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &bufferInfo;

    writes.push_back(write);
}

void DescriptorWriter::clear()
{
    imageInfos.clear();
    bufferInfos.clear();
    writes.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
    for (VkWriteDescriptorSet& write : writes) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, static_cast<uint32>(writes.size()), writes.data(), 0, nullptr);
}