#pragma once
#include <functional>
#include <vector>
#include <thread>
#include <future>
#include <vulkan/vulkan.h>

namespace parallel {

// A simple task that records commands into a command buffer
struct ParallelTask {
    std::function<void(VkCommandBuffer)> record;
};

// Utility to run N tasks in parallel, each with its own command buffer
inline void record_parallel(
    const std::vector<ParallelTask>& tasks,
    const std::vector<VkCommandBuffer>& commandBuffers)
{
    size_t count = tasks.size();
    std::vector<std::future<void>> futures;
    for (size_t i = 0; i < count; ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            tasks[i].record(commandBuffers[i]);
        }));
    }
    for (auto& f : futures) {
        f.get();
    }
}

} // namespace parallel
