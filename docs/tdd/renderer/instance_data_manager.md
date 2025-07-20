# Instance Data Manager Design Document

## Purpose
Manages, caches, and synchronizes per-instance data for Vulkan draw commands. Maintains an instance data array for every unique mesh/material (pipeline) key pair. Gathers instance data from mesh instances in the scene graph each frame, and only updates the GPU buffer when changes occur (add/remove/modify). Uses host-visible, permanently mapped GPU memory for fast updates.

---

## Responsibilities
- Gather instance data from mesh instances every frame
- Maintain instance data arrays per mesh/material (pipeline) key
- Detect changes (add/remove/modify) and synchronize GPU buffers as needed
- Store instance data in host-visible, permanently mapped GPU buffers
- Provide API for accessing instance data buffers and counts for draw commands
- Support efficient per-frame updates and draw submission

---

## Key Structures
```cpp
struct InstanceData {
    glm::mat4 model;
    VkDeviceAddress materialAddress;
    VkDeviceAddress globalParamsAddress;
    // ...other per-instance data
};

struct MeshMaterialKey {
    MeshHandle mesh;
    PipelineHandle pipeline;
    bool operator==(const MeshMaterialKey& rhs) const;
    // ...hash function for use in unordered_map
};
```

---

## Example API
```cpp
class InstanceDataManager {
public:
    InstanceDataManager(BufferAllocator* allocator);

    // Gather and cache instance data from scene mesh instances
    void GatherInstanceData(const std::vector<MeshInstance>& meshInstances);

    // Mark instance data as dirty for a given key
    void MarkDirty(const MeshMaterialKey& key);

    // Synchronize instance data buffer with GPU if dirty
    void SyncToGPU(const MeshMaterialKey& key);

    // Accessors for draw commands
    VkBuffer GetInstanceBuffer(const MeshMaterialKey& key) const;
    size_t GetInstanceCount(const MeshMaterialKey& key) const;
    const InstanceData* GetHostData(const MeshMaterialKey& key) const;

private:
    BufferAllocator* allocator_;
    struct InstanceBuffer {
        std::vector<InstanceData> hostData;
        VkBuffer gpuBuffer;
        InstanceData* mappedPtr;
        bool dirty;
    };
    std::unordered_map<MeshMaterialKey, InstanceBuffer> buffers_;
};
```

---

## Change Detection & Buffer Management Pseudocode

### 1. Gather and Detect Add/Remove/Modify

```cpp
void InstanceDataManager::GatherInstanceData(const std::vector<MeshInstance>& meshInstances) {
    // Group meshInstances by MeshMaterialKey
    std::unordered_map<MeshMaterialKey, std::vector<InstanceData>> newData;
    std::unordered_set<MeshMaterialKey> activeKeys;

    for (const auto& inst : meshInstances) {
        MeshMaterialKey key = {inst.mesh, inst.pipeline};
        InstanceData data = {/* fill from inst */};
        newData[key].push_back(data);
        activeKeys.insert(key);
    }

    // Detect additions (new keys)
    for (const auto& key : activeKeys) {
        if (buffers_.find(key) == buffers_.end()) {
            // New key: allocate buffer, mark dirty
            InstanceBuffer buf;
            buf.hostData = newData[key];
            buf.gpuBuffer = allocator_->CreateBuffer(buf.hostData.size() * sizeof(InstanceData), /*usage*/);
            buf.mappedPtr = allocator_->MapBuffer(buf.gpuBuffer);
            buf.dirty = true;
            buffers_[key] = buf;
        }
    }

    // Detect removals (keys no longer present)
    for (auto it = buffers_.begin(); it != buffers_.end(); ) {
        if (activeKeys.find(it->first) == activeKeys.end()) {
            allocator_->DestroyBuffer(it->second.gpuBuffer);
            it = buffers_.erase(it);
        } else {
            ++it;
        }
    }

    // Detect modifications (data changed for existing key)
    for (const auto& key : activeKeys) {
        auto& buf = buffers_[key];
        const auto& newInstances = newData[key];
        if (newInstances.size() != buf.hostData.size() ||
            !std::equal(newInstances.begin(), newInstances.end(), buf.hostData.begin())) {
            buf.hostData = newInstances;
            buf.dirty = true;
        }
    }
}
```

### 2. Sync to GPU

```cpp
void InstanceDataManager::SyncToGPU(const MeshMaterialKey& key) {
    auto it = buffers_.find(key);
    if (it == buffers_.end()) return;
    InstanceBuffer& buf = it->second;
    if (buf.dirty) {
        memcpy(buf.mappedPtr, buf.hostData.data(), buf.hostData.size() * sizeof(InstanceData));
        buf.dirty = false;
    }
}
```

---

## Typical Workflow
```cpp
// Per frame:
instanceDataManager->GatherInstanceData(meshInstances);
for (const auto& key : allKeys) {
    instanceDataManager->SyncToGPU(key);
}

// For draw command:
VkBuffer instanceBuffer = instanceDataManager->GetInstanceBuffer(key);
size_t instanceCount = instanceDataManager->GetInstanceCount(key);
```

---

## Notes
- Instance data is only uploaded to GPU when changes are detected for a given key (add/remove/modify)
- Host data is always available for CPU-side queries
- GPU buffers are permanently mapped for fast updates
- Supports efficient batching and draw submission per mesh/material
- This document should be updated as the instance data manager evolves
