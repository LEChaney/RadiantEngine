//> includes
#include "vk_engine.h"
#include "vk_images.h"
#include "fmt/core.h"
#include "vk_pipelines.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <cstdint>
#include <vk_initializers.h>
#include <vk_types.h>

#include "VkBootstrap.h"

#include "glm/gtx/transform.hpp"

#include <chrono>
#include <thread>

#include <iostream>
#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

constexpr bool bUseValidationLayers = true;

VulkanEngine* loadedEngine = nullptr;

// Frustum-AABB test in view space
static bool is_in_frustum(const RenderObject& obj, const std::array<FrustumPlane, 6>& planes, const glm::mat4& view) {
    glm::mat4 objectToView = view * obj.transform;
    glm::vec3 center_vs = glm::vec3(objectToView * glm::vec4(obj.bounds.origin, 1.0f));
    glm::mat3 absRotScale = glm::mat3(
        glm::abs(glm::vec3(objectToView[0])),
        glm::abs(glm::vec3(objectToView[1])),
        glm::abs(glm::vec3(objectToView[2]))
    );
    glm::vec3 ext_vs = absRotScale * obj.bounds.extents;
    for (const auto& plane : planes) {
        float r = ext_vs.x * std::abs(plane.normal.x) +
                  ext_vs.y * std::abs(plane.normal.y) +
                  ext_vs.z * std::abs(plane.normal.z);
        float d = glm::dot(plane.normal, center_vs) + plane.d;
        if (d + r < 0) {
            return false;
        }
    }
    return true;
}

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    init_vulkan();
    init_swapchain();
    init_commands();
    init_sync_structures();
    init_descriptors();
    init_pipelines();
    init_imgui();
    init_default_data();

    // everything went fine
    _isInitialized = true;
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer newBuffer;
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));
    return newBuffer;
}

void VulkanEngine::destroy_buffer(const AllocatedBuffer &buffer)
{
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

AllocatedImage VulkanEngine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.format = format;
    newImage.extent = size;

    VkImageCreateInfo imageInfo = vkinit::image_create_info(format, usage, size);
    if (mipmapped) {
        imageInfo.mipLevels = static_cast<uint32>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // always allocate image on dedicated GPU memory
    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    vmaAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // allocate and create the image
    VK_CHECK(vmaCreateImage(_allocator, &imageInfo, &vmaAllocInfo, &newImage.image, &newImage.allocation, nullptr));

    // if the format is a depth format, we will need to have it use the correct
    // aspect flag
    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D24_UNORM_S8_UINT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // build an image view for the image
    VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;

    VK_CHECK(vkCreateImageView(_device, &viewInfo, nullptr, &newImage.view));
    return newImage;
}

AllocatedImage VulkanEngine::create_image(void *rawData, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    // Create staging buffer
    // TODO: Handle sizes of non-RGBA8 textures
    size_t dataSize = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadBuffer = create_buffer(
        dataSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);

    // upload image data to staging buffer
    memcpy(uploadBuffer.allocation->GetMappedData(), rawData, dataSize);

    AllocatedImage newImage = create_image(
        size,
        format,
        usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        mipmapped
    );

    immediate_submit([&](VkCommandBuffer cmd) {
        vkutil::transition_image(
            cmd,
            newImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0; // Note: setting these to 0 uses imageExtent instead
        copyRegion.bufferImageHeight = 0; // Note: setting these to 0 uses imageExtent instead
        copyRegion.imageExtent = size;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;

        // copy the buffer into the image
        vkCmdCopyBufferToImage(
            cmd,
            uploadBuffer.buffer,
            newImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);

        if (mipmapped) {
            vkutil::generate_mipmaps(cmd, newImage.image, VkExtent2D(size.width, size.height));
        } else {
            vkutil::transition_image(
                cmd,
                newImage.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    destroy_buffer(uploadBuffer);

    return newImage;
}

void VulkanEngine::destroy_image(const AllocatedImage &image)
{
    vkDestroyImageView(_device, image.view, nullptr);
    vmaDestroyImage(_allocator, image.image, image.allocation);
}

GPUMeshBuffers VulkanEngine::upload_mesh(std::span<uint32> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32);

    GPUMeshBuffers newSurface;

    // create vertex buffer
    newSurface.vertexBuffer = create_buffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    // find the addres of the vertex buffer
    VkBufferDeviceAddressInfo deviceAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = newSurface.vertexBuffer.buffer
    };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

    // Create index buffer
    newSurface.indexBuffer = create_buffer(
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = create_buffer(
        vertexBufferSize + indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY
    );

    void* data = staging.allocation->GetMappedData();

    // copy buffers from cpu to gpu staging buffers
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy((uint8_t*)data + vertexBufferSize, indices.data(), indexBufferSize);

    // copy buffers to final gpu only buffers
    // TODO: Make this background task instead of blocking
    immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{ 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    // destroy intermediate staging buffer
    destroy_buffer(staging);

    return newSurface;
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {
        vkDeviceWaitIdle(_device);

        _loadedScenes.clear();

        // Free per-frame resources
        for (int i = 0; i < FRAME_OVERLAP; ++i)
        {
            for (int j = 0; j < NUM_PARALLEL_GEOMETRY_CMDS; ++j) {
                vkDestroyCommandPool(_device, _frames[i].geometryCommandPools[j], nullptr);
            }
            vkDestroyCommandPool(_device, _frames[i].commandPool, nullptr);

            // Destroy synchronization objects
            vkDestroyFence(_device, _frames[i].renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i].renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i].swapchainSemaphore, nullptr);

            _frames[i].deletionQueue.flush();
        }

        _metalRoughMaterial.destroy_pipelines(_device);

        _mainDeletionQueue.flush();

        destroy_swapchain();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);

        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);
        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    // Note: Must be set before updating the render scene
    _drawExtent.height = std::min(_swapchainExtent.height, _drawImage.extent.height) * _renderScale;
    _drawExtent.width = std::min(_swapchainExtent.width, _drawImage.extent.width) * _renderScale;

    update_render_scene();

    // Wait until the gpu has finished rendering the previous frame. Timeout of 1
    // second
    FrameData& frame = get_current_frame();
    VK_CHECK(vkWaitForFences(_device, 1, &frame.renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(_device, 1, &frame.renderFence));

    // Clean up any frame related resources that are no longer needed
    // Note: We need to wait for the frames associated render fence to be signaled before we can do this safely
    frame.deletionQueue.flush();
    frame.frameDescriptorAllocator.reset_pools(_device);

    // Request the next image from the swapchain
    uint32 swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, frame.swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        // The swapchain is out of date, we need to recreate it
        _resizeRequested = true;
        return;
    } else if (e != VK_SUBOPTIMAL_KHR) {
        VK_CHECK(e);
    }

    // Reset the command buffer before recording
    VkCommandBuffer cmd = frame.mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // Begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    // Note: By exactly once, we mean that it will not be submitted multiple times before being reset.
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    // Start recording commands
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(
        cmd,
        _drawImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED, // Don't care about the old layout
        VK_IMAGE_LAYOUT_GENERAL);

    draw_background(cmd);

    vkutil::transition_image(cmd, _drawImage.image,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transition_image(cmd, _depthImage.image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    draw_geometry(cmd);

    // transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(
        cmd,
        _drawImage.image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(
        cmd,
        _swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // copy the image to the swapchain image
    vkutil::copy_image_to_image(
        cmd,
        _drawImage.image,
        _swapchainImages[swapchainImageIndex],
        _drawExtent,
        _swapchainExtent);

    // Set swapchain image layout to Attachment Optimial so we can draw ImGui UI to it
    vkutil::transition_image(
        cmd,
        _swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // Draw ImGui UI
    draw_imgui(cmd, _swapchainImageViews[swapchainImageIndex]);

    // transition the swapchain image to present layout
    vkutil::transition_image(
        cmd,
        _swapchainImages[swapchainImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // End command buffer recording
    VK_CHECK(vkEndCommandBuffer(cmd));

    //prepare the submission to the queue.

    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);

    // prepare the wait and signal semaphores
    // we want to wait on the swapchainSemaphore, as that semaphore is signaled when the swapchain is ready
    // we will signal the renderSemaphore, to signal that rendering has finished
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        frame.swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
        VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        frame.renderSemaphore);

    VkSubmitInfo2 submitInfo = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);

    // Submit the command buffer to the queue
    // _renderFence will now block until the gpu has finished rendering
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, frame.renderFence));

    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;

    // we want to wait on the renderSemaphore, as that semaphore is signaled when rendering has finished
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frame.renderSemaphore;

    // submit the present request
    VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        // The swapchain is out of date, we need to recreate it
        _resizeRequested = true;
    } else {
        VK_CHECK(presentResult);
    }

    // increment frame number
    _frameNumber++;
}

void VulkanEngine::draw_background(VkCommandBuffer cmd)
{
    ComputeEffect& effect = _backgroundEffects[_currentBackgroundEffect];

    // Bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // Bind the descriptor set for the background effect
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout, 0, 1, &_backgroundDescriptorSet, 0, nullptr);

    // Setup push constants
    vkCmdPushConstants(cmd, _computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    // Dispatch the compute shader
    vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16.0f), std::ceil(_drawExtent.height / 16.0f), 1);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd)
{
    _stats.drawcall_count = 0;
    _stats.triangle_count = 0;
    auto start = std::chrono::system_clock::now();

    FrameData& frame = get_current_frame();

    // Set up the rendering info for the color attachment
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
        _drawImage.view,
        nullptr,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(
        _depthImage.view,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(
        _drawExtent,
        &colorAttachment,
        &depthAttachment);
    if (!singleThreadedGeometry) {
        renderInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT; // We will use secondary command buffers for rendering
    }

    // Begin render pass
    vkCmdBeginRendering(cmd, &renderInfo);

    // Dynamically allocate GPU Scene Data buffer (happens every frame)
    // TODO: Save and reuse the buffer instead of creating a new one every frame
    AllocatedBuffer gpuSceneDataBuffer = create_buffer(
        sizeof(GPUSceneData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
    // Ensure memory is freed when the frame is done
    frame.deletionQueue.push_function([=]() {
        destroy_buffer(gpuSceneDataBuffer);
    });

    // Populate the GPU scene data buffer
    GPUSceneData* mappedSceneData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
    *mappedSceneData = _sceneData;

    // Create a descriptor set for the scene data
    VkDescriptorSet sceneDataDescriptorSet = frame.frameDescriptorAllocator.allocate(
        _device, _gpuSceneDataDescriptorSetLayout);

    // Update the descriptor set with the scene data bindings
    DescriptorWriter writer;
    writer.write_buffer(
        0,
        gpuSceneDataBuffer.buffer,
        sizeof(GPUSceneData),
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(_device, sceneDataDescriptorSet);

    // Frustum cull and sort opaque draws to group render state
    std::vector<uint32_t> opaque_draws;
    opaque_draws.reserve(_mainDrawContext.OpaqueDrawData.size());
    for (uint32_t i = 0; i < _mainDrawContext.OpaqueDrawData.size(); i++) {
        if (is_in_frustum(_mainDrawContext.OpaqueDrawData[i], _mainCamera.getFrustumPlanesVS(), _sceneData.view)) {
            opaque_draws.push_back(i);
        }
    }
    // sort the opaque surfaces by material and mesh
    std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
        const RenderObject& A = _mainDrawContext.OpaqueDrawData[iA];
        const RenderObject& B = _mainDrawContext.OpaqueDrawData[iB];

        // Sort by material pipeline first
        if (A.material->pipeline != B.material->pipeline) {
            return A.material->pipeline < B.material->pipeline;
        }
        // Then by material descriptor set
        if (A.material->descriptorSet != B.material->descriptorSet) {
            return A.material->descriptorSet < B.material->descriptorSet;
        }
        // Then by mesh index buffer
        return A.indexBuffer < B.indexBuffer;
    });

    // Frustum cull and sort transparent draws for correct blending
    std::vector<uint32> transparent_draws;
    transparent_draws.reserve(_mainDrawContext.TransparentDrawData.size());
    for (uint32 i = 0; i < _mainDrawContext.TransparentDrawData.size(); i++) {
        if (is_in_frustum(_mainDrawContext.TransparentDrawData[i], _mainCamera.getFrustumPlanesVS(), _sceneData.view)) {
            transparent_draws.push_back(i);
        }
    }
    // sort the transparent surfaces by distance to camera
    std::sort(transparent_draws.begin(), transparent_draws.end(), [&](const auto& iA, const auto& iB) {
        const RenderObject& A = _mainDrawContext.TransparentDrawData[iA];
        const RenderObject& B = _mainDrawContext.TransparentDrawData[iB];
        return glm::length(glm::vec3(A.transform[3]) - _mainCamera.position) >
               glm::length(glm::vec3(B.transform[3]) - _mainCamera.position);
    });

    // Combine all draws into a single list (opaque first, then transparent)
    std::vector<std::pair<bool, uint32_t>> all_draws; // bool: true=opaque, false=transparent
    all_draws.reserve(opaque_draws.size() + transparent_draws.size());
    // Add all opaque draws first
    for (auto i : opaque_draws) all_draws.emplace_back(true, i);
    // Then add all transparent draws
    for (auto i : transparent_draws) all_draws.emplace_back(false, i);

    // Consolidated draw function for both single-threaded and parallel paths
    auto draw_renderobjects = [&](VkCommandBuffer drawCmd, const std::vector<std::pair<bool, uint32_t>>& draws, MaterialInstance** pLastMaterial, VkBuffer* pLastIndexBuffer) {
        for (const auto& drawinfo : draws) {
            const RenderObject& r = drawinfo.first ? _mainDrawContext.OpaqueDrawData[drawinfo.second] : _mainDrawContext.TransparentDrawData[drawinfo.second];
            if (r.material != *pLastMaterial) {
                if (!*pLastMaterial || r.material->pipeline != (*pLastMaterial)->pipeline) {
                    vkCmdBindPipeline(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->pipeline);
                }
                if (!*pLastMaterial || r.material->pipeline->layout != (*pLastMaterial)->pipeline->layout) {
                    vkCmdBindDescriptorSets(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 0, 1, &sceneDataDescriptorSet, 0, nullptr);
                    vkCmdBindDescriptorSets(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1, &r.material->descriptorSet, 0, nullptr);
                } else if (!*pLastMaterial || r.material->descriptorSet != (*pLastMaterial)->descriptorSet) {
                    vkCmdBindDescriptorSets(drawCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, r.material->pipeline->layout, 1, 1, &r.material->descriptorSet, 0, nullptr);
                }
                *pLastMaterial = r.material;
            }
            if (r.indexBuffer != *pLastIndexBuffer) {
                vkCmdBindIndexBuffer(drawCmd, r.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                *pLastIndexBuffer = r.indexBuffer;
            }
            MeshDrawPushConstants pushConstants;
            pushConstants.vertexBuffer = r.vertexBufferAddress;
            pushConstants.worldMatrix = r.transform;
            vkCmdPushConstants(drawCmd, r.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshDrawPushConstants), &pushConstants);
            vkCmdDrawIndexed(drawCmd, r.indexCount, 1, r.firstIndex, 0, 0);
        }
    };

    if (singleThreadedGeometry) {
        // Single-threaded: record all draws directly to main command buffer
        MaterialInstance* lastMaterial = nullptr;
        VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
        draw_renderobjects(cmd, all_draws, &lastMaterial, &lastIndexBuffer);
        // End render pass
        vkCmdEndRendering(cmd);
        _mainDrawContext.OpaqueDrawData.clear();
        _mainDrawContext.TransparentDrawData.clear();
        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        _stats.mesh_CPU_draw_time = elapsed.count() / 1000.f;
        return;
    }

    // Partition draws for parallel recording
    size_t total_draws = all_draws.size();
    size_t chunk = (total_draws + NUM_PARALLEL_GEOMETRY_CMDS - 1) / NUM_PARALLEL_GEOMETRY_CMDS;
    std::vector<Parallel::ParallelTask> tasks;
    for (unsigned int t = 0; t < NUM_PARALLEL_GEOMETRY_CMDS; ++t) {
        size_t begin = t * chunk;
        size_t end = std::min(begin + chunk, total_draws);
        if (begin >= end) break;
        tasks.push_back(Parallel::ParallelTask{
            [this, &all_draws, begin, end, &sceneDataDescriptorSet, &frame, &draw_renderobjects](VkCommandBuffer subcmd) {
                // Begin secondary command buffer
                VkCommandBufferInheritanceRenderingInfo inheritanceRenderingInfo{};
                inheritanceRenderingInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
                inheritanceRenderingInfo.colorAttachmentCount = 1;
                inheritanceRenderingInfo.pColorAttachmentFormats = &_drawImage.format;
                inheritanceRenderingInfo.depthAttachmentFormat = _depthImage.format;
                inheritanceRenderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED; // No stencil attachment
                inheritanceRenderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
                
                VkCommandBufferInheritanceInfo inheritInfo{};
                inheritInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
                inheritInfo.pNext = &inheritanceRenderingInfo;
                
                // No framebuffer, dynamic rendering
                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                beginInfo.pInheritanceInfo = &inheritInfo;
                VK_CHECK(vkResetCommandBuffer(subcmd, 0));
                VK_CHECK(vkBeginCommandBuffer(subcmd, &beginInfo));

                // Set dynamic viewport
                VkViewport viewport = {};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(_drawExtent.width);
                viewport.height = static_cast<float>(_drawExtent.height);
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(subcmd, 0, 1, &viewport);

                // Set dynamic scissor
                VkRect2D scissor = {};
                scissor.offset = { 0, 0 };
                scissor.extent.width = _drawExtent.width;
                scissor.extent.height = _drawExtent.height;
                vkCmdSetScissor(subcmd, 0, 1, &scissor);

                MaterialInstance* lastMaterial = nullptr;
                VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
                // Only draw the subrange for this task
                std::vector<std::pair<bool, uint32_t>> sub_draws(all_draws.begin() + begin, all_draws.begin() + end);
                draw_renderobjects(subcmd, sub_draws, &lastMaterial, &lastIndexBuffer);
                
                VK_CHECK(vkEndCommandBuffer(subcmd));
            }
        });
    }
    // Record in parallel
    Parallel::recordParallel(tasks, std::vector<VkCommandBuffer>(frame.geometryCommandBuffers, frame.geometryCommandBuffers + tasks.size()));

    // Execute all secondary command buffers from the main command buffer
    std::vector<VkCommandBuffer> secondary_cmds(tasks.size());
    for (size_t i = 0; i < tasks.size(); ++i) {
        secondary_cmds[i] = frame.geometryCommandBuffers[i];
    }
    if (!secondary_cmds.empty()) {
        vkCmdExecuteCommands(cmd, static_cast<uint32_t>(secondary_cmds.size()), secondary_cmds.data());
    }

    // End render pass
    vkCmdEndRendering(cmd);
    
    _mainDrawContext.OpaqueDrawData.clear();
    _mainDrawContext.TransparentDrawData.clear();

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    _stats.mesh_CPU_draw_time = elapsed.count() / 1000.f;
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    // Set up attachment info for the rendering
     VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
        targetImageView,
        nullptr,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(
        _swapchainExtent,
        &colorAttachment,
        nullptr);

    // Record ImGui draw commands (generated by ImGui::Render())
    vkCmdBeginRendering(cmd, &renderInfo); // Needed because of dynamic rendering
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void VulkanEngine::update_render_scene()
{
    //begin clock
    auto start = std::chrono::system_clock::now();

    // Cache last used draw extent for projection/frustum
    static VkExtent2D lastDrawExtent = {0, 0};
    static glm::mat4 cachedProj;
    static std::array<FrustumPlane, 6> cachedFrustumPlanes;

    _mainCamera.update();
    _mainCamera.updateProjectionAndFrustum(_drawExtent, 70.f, 10000.f, 0.1f);
    glm::mat4 view = _mainCamera.getViewMatrix();
    glm::mat4 proj = _mainCamera.getProjection();

    _sceneData.view = view;
    _sceneData.proj = proj;
    _sceneData.viewproj = proj * view;

    // some default lighting parameters
    _sceneData.ambientColor = glm::vec4(.1f);
    _sceneData.sunlightColor = glm::vec4(1.f);
    _sceneData.sunlightDir = glm::vec4(0, 1, 0.5f, 1);

    _loadedScenes[_selectedMap]->gather_draw_data(
        glm::mat4(1),
        _mainDrawContext
    );

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    _stats.scene_update_time = elapsed.count() / 1000.f;
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)> &&function)
{
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    function(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdSubmitInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submitInfo = vkinit::submit_info(&cmdSubmitInfo, nullptr, nullptr);
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, _immFence));
    VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 1000000000));
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        //begin clock
        auto start = std::chrono::system_clock::now();

        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }

            // handle imgui events
            _mainCamera.processSDLEvent(e);
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (_resizeRequested) {
            resize_swapchain();
        }

        // start the imgui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ImGui UI for stats
        ImGui::Begin("Stats");

        ImGui::Text("frametime %f ms", _stats.frametime);
        ImGui::Text("CPU draw time %f ms", _stats.mesh_CPU_draw_time);
        ImGui::Text("update time %f ms", _stats.scene_update_time);
        ImGui::Text("triangles %i", _stats.triangle_count);
        ImGui::Text("draws %i", _stats.drawcall_count);

        // Static arrays to store history for graphs
        static float frametime_history[120] = {};
        static float drawtime_history[120] = {};
        static float updatetime_history[120] = {};
        static int history_offset = 0;

        frametime_history[history_offset] = _stats.frametime;
        drawtime_history[history_offset] = _stats.mesh_CPU_draw_time;
        updatetime_history[history_offset] = _stats.scene_update_time;
        history_offset = (history_offset + 1) % IM_ARRAYSIZE(frametime_history);

        ImGui::PlotLines("Frame Time (ms)", frametime_history, IM_ARRAYSIZE(frametime_history), history_offset, nullptr, 0.0f, 50.0f, ImVec2(0, 60));
        ImGui::PlotLines("Draw Time (ms)", drawtime_history, IM_ARRAYSIZE(drawtime_history), history_offset, nullptr, 0.0f, 20.0f, ImVec2(0, 60));
        ImGui::PlotLines("Update Time (ms)", updatetime_history, IM_ARRAYSIZE(updatetime_history), history_offset, nullptr, 0.0f, 20.0f, ImVec2(0, 60));

        ImGui::End();

        // UI for the background effect
        if (ImGui::Begin("Settings")) {
            ImGui::SliderFloat("Render Scale", &_renderScale, 0.3f, 1.0f);

            // Camera speed slider
            ImGui::SliderFloat("Camera Speed", &_mainCamera.movementSpeed, 0.01f, 10.0f, "%.2f");

            // Map selection combo box
            static int current_map_index = 0;
            std::vector<const char*> map_names;
            map_names.reserve(_loadedScenes.size());
            for (const auto& pair : _loadedScenes) {
                map_names.push_back(pair.first.c_str());
            }
            if (!_loadedScenes.empty()) {
                ImGui::Combo("Map", &current_map_index, map_names.data(), static_cast<int>(map_names.size()));
                _selectedMap = map_names[current_map_index];
            }

            ComputeEffect& selected = _backgroundEffects[_currentBackgroundEffect];

            ImGui::Text("Selected effect: %s", selected.name);

            ImGui::SliderInt("Effect Index", &_currentBackgroundEffect, 0, _backgroundEffects.size() - 1);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);
        }
        ImGui::End();

        // Generate ImGui render commands (doesn't actually draw anything)
        ImGui::Render();

        // Our draw function
        draw();

        //get clock again, compare with start clock
        auto end = std::chrono::system_clock::now();

        //convert to microseconds (integer), and then come back to miliseconds
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        _stats.frametime = elapsed.count() / 1000.f;
    }
}

void VulkanEngine::init_vulkan()
{
    /******* Initialize vulkan instance *******/
    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name("Example Vulkan Application")
    .request_validation_layers(bUseValidationLayers)
    .use_default_debug_messenger()
    .require_api_version(1, 3, 0)
    .build();

    vkb::Instance vkb_inst = inst_ret.value();

    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    /******* Initialize vulkan device *******/
    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    // Vulkan 1.3 features (simplify rendering setup and synchonization)
    VkPhysicalDeviceVulkan13Features features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features.dynamicRendering = true;
    features.synchronization2 = true;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    // Enumerate physical devices
    uint32_t gpuCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(_instance, &gpuCount, nullptr));
    if (gpuCount == 0) {
        throw std::runtime_error("No Vulkan-compatible GPUs found.");
    }
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    VK_CHECK(vkEnumeratePhysicalDevices(_instance, &gpuCount, physicalDevices.data()));

    // Pick first suitable discrete GPU
    VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties selectedProps{};
    if (selectedDevice == VK_NULL_HANDLE) {
        for (const auto& device : physicalDevices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(device, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                selectedDevice = device;
                selectedProps = props;
                break;
            }
        }
    }
    if (selectedDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable discrete GPU found.");
    }

    // Query queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(selectedDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(selectedDevice, &queueFamilyCount, queueFamilies.data());

    // Find a queue family that supports graphics and present
    int graphicsQueueFamily = -1;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(selectedDevice, i, _surface, &presentSupport);
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
            graphicsQueueFamily = i;
            break;
        }
    }
    if (graphicsQueueFamily == -1) {
        throw std::runtime_error("No suitable graphics queue family found.");
    }

    // Enable required device features
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = graphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // Enable Vulkan 1.3 and 1.2 features
    features.pNext = &features12;

    // Required device extensions
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Create logical device
    VkDevice device;
    VK_CHECK(vkCreateDevice(selectedDevice, &deviceCreateInfo, nullptr, &device));

    // Save handles
    _device = device;
    _chosenGPU = selectedDevice;
    _graphicsQueueFamily = graphicsQueueFamily;

    // Get graphics queue
    vkGetDeviceQueue(_device, _graphicsQueueFamily, 0, &_graphicsQueue);

    // Save vk handles
    // _device = vkbDevice.device;
    // _chosenGPU = physicalDevice.physical_device;

    // /******* Initializing queue *******/
    // _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    // _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    /******* Initialize the memory allocator *******/
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _chosenGPU;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    _mainDeletionQueue.push_function([&]() {
        vmaDestroyAllocator(_allocator);
    });

    // Print selected GPU device properties
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(_chosenGPU, &deviceProperties);

    // Convert VkPhysicalDeviceType to string
    const char* deviceTypeStr = "Unknown";
    switch (deviceProperties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER: deviceTypeStr = "Other"; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceTypeStr = "Integrated GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceTypeStr = "Discrete GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceTypeStr = "Virtual GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceTypeStr = "CPU"; break;
    }
    
    // Print device properties
    fmt::print("Selected GPU Device:\n");
    fmt::print("  Name: {}\n", deviceProperties.deviceName);
    fmt::print("  Type: {}\n", deviceTypeStr);
    fmt::print("  API Version: {}.{}.{}\n",
        VK_VERSION_MAJOR(deviceProperties.apiVersion),
        VK_VERSION_MINOR(deviceProperties.apiVersion),
        VK_VERSION_PATCH(deviceProperties.apiVersion));
    fmt::print("  Driver Version: {}\n", deviceProperties.driverVersion);
    fmt::print("  Vendor ID: {:#06x}\n", deviceProperties.vendorID);
    fmt::print("  Device ID: {:#06x}\n", deviceProperties.deviceID);

    // Query physical device memory properties
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(_chosenGPU, &memProperties);

    fmt::print("Available VkMemoryHeaps:\n");
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; ++i) {
        VkDeviceSize heapSizeMB = memProperties.memoryHeaps[i].size / (1024 * 1024);
        VkMemoryHeapFlags flags = memProperties.memoryHeaps[i].flags;

        std::string flagStr;
        if (flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) flagStr += "DEVICE_LOCAL ";
        if (flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) flagStr += "MULTI_INSTANCE ";
#if defined(VK_VERSION_1_2)
        if (flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT_KHR) flagStr += "MULTI_INSTANCE_KHR ";
#endif

        if (flagStr.empty()) flagStr = "None";

        fmt::print("  Heap {}: {} MB [{}]\n", i, heapSizeMB, flagStr);
    }

    fmt::print("Available VkMemoryTypes:\n");
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        const VkMemoryType& type = memProperties.memoryTypes[i];
        std::string propertyStr;
        if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) propertyStr += "DEVICE_LOCAL ";
        if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) propertyStr += "HOST_VISIBLE ";
        if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) propertyStr += "HOST_COHERENT ";
        if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) propertyStr += "HOST_CACHED ";
        if (type.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) propertyStr += "LAZILY_ALLOCATED ";
#if defined(VK_VERSION_1_1)
        if (type.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) propertyStr += "PROTECTED ";
        if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) propertyStr += "DEVICE_COHERENT_AMD ";
        if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD) propertyStr += "DEVICE_UNCACHED_AMD ";
#endif
        if (propertyStr.empty()) propertyStr = "None";
        fmt::print("  Type {}: Heap {} [{}]\n", i, type.heapIndex, propertyStr);
    }
}

void VulkanEngine::init_swapchain()
{
    create_swapchain(_windowExtent.width, _windowExtent.height);

    SDL_DisplayMode displayMode;
    if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0) {
        throw std::runtime_error("Failed to get current display mode");
    }

    VkExtent3D drawImageExtent = {
        static_cast<uint32_t>(displayMode.w),
        static_cast<uint32_t>(displayMode.h),
        1
    };

    VkImageUsageFlags drawImageUsage{};
    drawImageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    drawImageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImageUsageFlags depthImageUsage{};
    depthImageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkFormat drawImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat depthImageFormat = VK_FORMAT_D32_SFLOAT;
    VkImageCreateInfo rimg_info = vkinit::image_create_info(
        drawImageFormat,
        drawImageUsage,
        drawImageExtent);
    VkImageCreateInfo dimg_info = vkinit::image_create_info(
        depthImageFormat,
        depthImageUsage,
        drawImageExtent);

    // For the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocInfo = {};
    rimg_allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate and create the single draw and depth images
    _drawImage.extent = drawImageExtent;
    _depthImage.extent = drawImageExtent;
    _drawImage.format = drawImageFormat;
    _depthImage.format = depthImageFormat;

    vmaCreateImage(
        _allocator,
        &rimg_info,
        &rimg_allocInfo,
        &_drawImage.image,
        &_drawImage.allocation,
        nullptr);
    vmaCreateImage(
        _allocator,
        &dimg_info,
        &rimg_allocInfo,
        &_depthImage.image,
        &_depthImage.allocation,
        nullptr);

    // Create the image view for the draw image
    VkImageViewCreateInfo rimg_view_info = vkinit::imageview_create_info(
        _drawImage.format,
        _drawImage.image,
        VK_IMAGE_ASPECT_COLOR_BIT);
    VkImageViewCreateInfo dimg_view_info = vkinit::imageview_create_info(
        _depthImage.format,
        _depthImage.image,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_device, &rimg_view_info, nullptr, &_drawImage.view));
    VK_CHECK(vkCreateImageView(_device, &dimg_view_info, nullptr, &_depthImage.view));

    // Add the image and view to the deletion queue
    _mainDeletionQueue.push_function([=]() {
        vkDestroyImageView(_device, _drawImage.view, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);
        vkDestroyImageView(_device, _depthImage.view, nullptr);
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
    });
}

void VulkanEngine::init_commands()
{
    // Create a command pool for commands submitted to the graphics queue.
    // We also want to pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; ++i)
    {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i].commandPool));

        // Allocate the default command buffer that we will use fo rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i].commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i].mainCommandBuffer));

        // Create geometry command pools and buffers
        for (unsigned int j = 0; j < NUM_PARALLEL_GEOMETRY_CMDS; ++j) {
            VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i].geometryCommandPools[j]));
            VkCommandBufferAllocateInfo geomCmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i].geometryCommandPools[j], 1);
            geomCmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
            VK_CHECK(vkAllocateCommandBuffers(_device, &geomCmdAllocInfo, &_frames[i].geometryCommandBuffers[j]));
        }
    }

    // Create a command pool for the immediate command buffer
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));

    // Allocate the command buffer that we will use for immediate submission
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

    _mainDeletionQueue.push_function([&]() {
        vkDestroyCommandPool(_device, _immCommandPool, nullptr);
    });
}

void VulkanEngine::init_sync_structures()
{
    //create syncronization structures
    //one fence to control when the gpu has finished rendering the frame,
    //and 2 semaphores to syncronize rendering with swapchain
    //we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < FRAME_OVERLAP; ++i)
    {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i].renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].renderSemaphore));
    }

    // Create a fence for the immediate command buffer
    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
    _mainDeletionQueue.push_function([&]() {
        vkDestroyFence(_device, _immFence, nullptr);
    });
}

void VulkanEngine::create_swapchain(uint32 width, uint32 height)
{
    // Query the number of supported formats
    // uint32_t formatCount;
    // vkGetPhysicalDeviceSurfaceFormatsKHR(_chosenGPU, _surface, &formatCount, nullptr);

    // // Allocate space for the formats
    // std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);

    // // Retrieve the supported formats
    // vkGetPhysicalDeviceSurfaceFormatsKHR(_chosenGPU, _surface, &formatCount, surfaceFormats.data());

    // // Print the supported formats and color spaces
    // for (const auto& surfaceFormat : surfaceFormats) {
    //     fmt::print("Format: {}, Color Space: {}\n",
    //                vkutil::vk_format_to_string(surfaceFormat.format),
    //                vkutil::vk_color_space_to_string(surfaceFormat.colorSpace));
    // }

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{.format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    _swapchainExtent = vkbSwapchain.extent;
    _swapchain = vkbSwapchain.swapchain;
    _swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::destroy_swapchain()
{
    // Note: also destroys associated VkImages but not VkImageViews
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    for (int i = 0; i < _swapchainImageViews.size(); ++i)
    {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
}

void VulkanEngine::init_descriptors()
{
    // Create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> poolRatios = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.0f}
    };

    _globalDescriptorAllocator.init(_device, 10, poolRatios);

    // Create the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _backgroundDescriptorSetLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    // Create the descriptor set layout for gpu scene data
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        _gpuSceneDataDescriptorSetLayout = builder.build(
            _device,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
        );
    }

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _singleImageDescriptorSetLayout = builder.build(_device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    // Allocate and update the single _backgroundDescriptorSet
    _backgroundDescriptorSet = _globalDescriptorAllocator.allocate(_device, _backgroundDescriptorSetLayout);
    DescriptorWriter writer;
    writer.write_image(0, _drawImage.view, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(_device, _backgroundDescriptorSet);

    for (int i = 0; i < FRAME_OVERLAP; ++i)
    {
        // Initialize the per frame descriptor set allocator
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4}
        };

        _frames[i].frameDescriptorAllocator = DescriptorAllocatorGrowable{};
        _frames[i].frameDescriptorAllocator.init(_device, 1000, frameSizes);

        _mainDeletionQueue.push_function([=]() {
            _frames[i].frameDescriptorAllocator.destroy_pools(_device);
        });
    }

    // Make sure both the descriptor allocator and the new laytout get cleaned up properly
    _mainDeletionQueue.push_function([=]() {
        _globalDescriptorAllocator.destroy_pools(_device);
        vkDestroyDescriptorSetLayout(_device, _backgroundDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _gpuSceneDataDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _singleImageDescriptorSetLayout, nullptr);
    });
}

void VulkanEngine::init_pipelines()
{
    init_background_pipelines();

    _metalRoughMaterial.build_pipelines(this);
}

void VulkanEngine::init_background_pipelines()
{
    // Create pipeline layout for the gradient pipeline
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = nullptr;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &_backgroundDescriptorSetLayout;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ComputePushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_computePipelineLayout));

    // Load the shader modules
    VkShaderModule gradientShader;
    if (!vkutil::load_shader_module(_device, "shaders/gradient_color.comp.spv", &gradientShader)) {
        throw std::runtime_error("Failed to load gradient color shader module");
    }
    VkShaderModule skyShader;
    if (!vkutil::load_shader_module(_device, "shaders/sky.comp.spv", &skyShader)) {
        throw std::runtime_error("Failed to load sky shader module");
    }

    // Create the pipelines
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.pNext = nullptr;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = gradientShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = nullptr;
    pipelineCreateInfo.stage = stageInfo;
    pipelineCreateInfo.layout = _computePipelineLayout;

    // Setup gradient effect data
    ComputeEffect gradient;
    gradient.layout = _computePipelineLayout;
    gradient.name = "Gradient";
    gradient.data = {};
    // default colors
    gradient.data.data1 = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
    gradient.data.data2 = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &gradient.pipeline));

    pipelineCreateInfo.stage.module = skyShader;

    // Setup sky effect data
    ComputeEffect sky;
    sky.layout = _computePipelineLayout;
    sky.name = "Sky";
    sky.data = {};
    // default sky parameters
    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &sky.pipeline));

    // Add the 2 backgound effects into the array
    _backgroundEffects.push_back(gradient);
    _backgroundEffects.push_back(sky);

    // Clean up the shader module (We can delete it now that the pipeline is created)
    vkDestroyShaderModule(_device, gradientShader, nullptr);
    vkDestroyShaderModule(_device, skyShader, nullptr);

    _mainDeletionQueue.push_function([=]() {
        vkDestroyPipelineLayout(_device, _computePipelineLayout, nullptr);
        vkDestroyPipeline(_device, sky.pipeline, nullptr);
        vkDestroyPipeline(_device, gradient.pipeline, nullptr);
    });
}

void VulkanEngine::init_imgui()
{
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.pNext = nullptr;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = static_cast<uint32>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    VkDescriptorPool imguiDescriptorPool; // Deletion queue owns handle after this
    VK_CHECK(vkCreateDescriptorPool(_device, &poolInfo, nullptr, &imguiDescriptorPool));

    // 2: Initialize ImGui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(_window);

    // this initializes imgui for vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _chosenGPU;
    init_info.Device = _device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = imguiDescriptorPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    // dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = {};
    init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    init_info.PipelineRenderingCreateInfo.pNext = nullptr;
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;

    ImGui_ImplVulkan_Init(&init_info);

    ImGui_ImplVulkan_CreateFontsTexture();

    _mainDeletionQueue.push_function([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, imguiDescriptorPool, nullptr);
    });
}

void VulkanEngine::init_default_data()
{
    // 3 default textures, white grey, black. 1 pixel each
    uint32 white = glm::packUnorm4x8(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    uint32 grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1.0f));
    uint32 black = glm::packUnorm4x8(glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    uint32 magenta = glm::packUnorm4x8(glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
    std::array<uint32, 16 * 16> checker;
    for (int32 x = 0; x < 16; ++x) {
        for (int32 y = 0; y < 16; ++y) {
            checker[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    _whiteImage = create_image(
        static_cast<void*>(&white),
        VkExtent3D{1, 1, 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT
    );
    _greyImage = create_image(
        static_cast<void*>(&grey),
        VkExtent3D{1, 1, 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT
    );
    _blackImage = create_image(
        static_cast<void*>(&black),
        VkExtent3D{1, 1, 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT
    );
    _errorCheckerboardImage = create_image(
        static_cast<void*>(checker.data()),
        VkExtent3D{16, 16, 1},
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT
    );

    // Create the default texture samplers
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(_device, &samplerInfo, nullptr, &_defaultSamplerNearest);

    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_device, &samplerInfo, nullptr, &_defaultSamplerLinear);

    _mainDeletionQueue.push_function([=]() {
        vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
        vkDestroySampler(_device, _defaultSamplerLinear, nullptr);

        destroy_image(_whiteImage);
        destroy_image(_greyImage);
        destroy_image(_blackImage);
        destroy_image(_errorCheckerboardImage);
    });

    // TODO: Move this into camera init method
    _mainCamera.velocity = glm::vec3(0.f);
    _mainCamera.position = glm::vec3(0.f, 0.f, 5.f);
    _mainCamera.pitch = 0.f;
    _mainCamera.yaw = 0.f;
    
    // Load default map
    std::string structurePath = { "assets/structure.glb" };
    auto structureFile = load_gltf(this, structurePath);
    if (structureFile.has_value()) {
        _loadedScenes["structure"] = *structureFile;
    }

    std::string basicMeshPath = { "assets/basicmesh.glb" };
    auto basicMeshFile = load_gltf(this, basicMeshPath);
    if (basicMeshFile.has_value()) {
        _loadedScenes["basicmesh"] = *basicMeshFile;
        _loadedScenes["basicmesh"]->delete_all_nodes_except("Suzanne");
    }

    std::string sponzaMeshPath = { "assets/sponza/Sponza.gltf" };
    auto sponzaGltf = load_gltf(this, sponzaMeshPath);
    if (sponzaGltf.has_value()) {
        _loadedScenes["sponza"] = *sponzaGltf;
    }

    std::string bistroMeshPath = { "assets/bistro.glb" };
    auto bistroGltf = load_gltf(this, bistroMeshPath);
    if (bistroGltf.has_value()) {
        _loadedScenes["bistro"] = *bistroGltf;
    }

    // Manually create a scene with RGB axis arrows
    {
        // Create vertices for 3 arrows (X: red, Y: green, Z: blue)
        // Cube arrow geometry for X (red), Y (green), Z (blue) axes
        // Each arrow is a thin box from (0,0,0) to (1,0.05,0.05) for X,
        // (0,0,0) to (0.05,1,0.05) for Y, (0,0,0) to (0.05,0.05,1) for Z

        std::vector<Vertex> axisVertices;

        // X axis (red)
        glm::vec4 red(1,0,0,1);
        // 8 corners of the X arrow box
        glm::vec3 x0(0, -0.025f, -0.025f);
        glm::vec3 x1(1, -0.025f, -0.025f);
        glm::vec3 x2(1,  0.025f, -0.025f);
        glm::vec3 x3(0,  0.025f, -0.025f);
        glm::vec3 x4(0, -0.025f,  0.025f);
        glm::vec3 x5(1, -0.025f,  0.025f);
        glm::vec3 x6(1,  0.025f,  0.025f);
        glm::vec3 x7(0,  0.025f,  0.025f);

        // Y axis (green)
        glm::vec4 green(0,1,0,1);
        glm::vec3 y0(-0.025f, 0, -0.025f);
        glm::vec3 y1(-0.025f, 1, -0.025f);
        glm::vec3 y2( 0.025f, 1, -0.025f);
        glm::vec3 y3( 0.025f, 0, -0.025f);
        glm::vec3 y4(-0.025f, 0,  0.025f);
        glm::vec3 y5(-0.025f, 1,  0.025f);
        glm::vec3 y6( 0.025f, 1,  0.025f);
        glm::vec3 y7( 0.025f, 0,  0.025f);

        // Z axis (blue)
        glm::vec4 blue(0,0,1,1);
        glm::vec3 z0(-0.025f, -0.025f, 0);
        glm::vec3 z1(-0.025f, -0.025f, 1);
        glm::vec3 z2( 0.025f, -0.025f, 1);
        glm::vec3 z3( 0.025f, -0.025f, 0);
        glm::vec3 z4(-0.025f,  0.025f, 0);
        glm::vec3 z5(-0.025f,  0.025f, 1);
        glm::vec3 z6( 0.025f,  0.025f, 1);
        glm::vec3 z7( 0.025f,  0.025f, 0);

        // Helper lambda to add a box (cube) to the vertex/index arrays
        auto add_box = [&](glm::vec3 v[8], glm::vec4 color, std::vector<Vertex>& verts, std::vector<uint32_t>& inds) {
            uint32_t base = static_cast<uint32_t>(verts.size());
            // Normals for each face
            glm::vec3 normals[6] = {
            { 1, 0, 0}, {-1, 0, 0},
            { 0, 1, 0}, { 0,-1, 0},
            { 0, 0, 1}, { 0, 0,-1}
            };
            // Each face: 4 indices (quad), two triangles
            uint32_t faces[6][4] = {
            {1,2,6,5}, // +X
            {0,4,7,3}, // -X
            {2,3,7,6}, // +Y
            {0,1,5,4}, // -Y
            {4,5,6,7}, // +Z
            {0,3,2,1}  // -Z
            };
            for (int f = 0; f < 6; ++f) {
            // 4 verts per face
            for (int vi = 0; vi < 4; ++vi) {
                glm::vec3 pos = v[faces[f][vi]];
                float uv_x = (vi == 1 || vi == 2) ? 1.0f : 0.0f;
                float uv_y = (vi >= 2) ? 1.0f : 0.0f;
                verts.push_back(Vertex{pos, uv_x, normals[f], uv_y, color});
            }
            // Indices for two triangles
            inds.push_back(base + f*4 + 0);
            inds.push_back(base + f*4 + 1);
            inds.push_back(base + f*4 + 2);
            inds.push_back(base + f*4 + 2);
            inds.push_back(base + f*4 + 3);
            inds.push_back(base + f*4 + 0);
            }
        };

        std::vector<uint32_t> axisIndices;
        // Add X arrow
        glm::vec3 xverts[8] = {x0,x1,x2,x3,x4,x5,x6,x7};
        add_box(xverts, red, axisVertices, axisIndices);
        // Add Y arrow
        glm::vec3 yverts[8] = {y0,y1,y2,y3,y4,y5,y6,y7};
        add_box(yverts, green, axisVertices, axisIndices);
        // Add Z arrow
        glm::vec3 zverts[8] = {z0,z1,z2,z3,z4,z5,z6,z7};
        add_box(zverts, blue, axisVertices, axisIndices);

        // Upload mesh to GPU
        GPUMeshBuffers meshBuffers = upload_mesh(axisIndices, axisVertices);

        // Create a dummy bounds for the arrows
        Bounds bounds;
        bounds.origin = glm::vec3(0.5f, 0.5f, 0.5f);
        bounds.extents = glm::vec3(1.0f);

        // Create a dummy material for the axes (using white texture, colored by vertex color)
        GLTFMetallicRoughness::MaterialResources matRes;
        matRes.dataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU).buffer;
        matRes.colorImage = _whiteImage;
        matRes.colorSampler = _defaultSamplerLinear;
        matRes.metalRoughImage = _whiteImage;
        matRes.metalRoughSampler = _defaultSamplerLinear;

        // Use opaque pipeline for all arrows
        MaterialInstance axisMaterial = _metalRoughMaterial.write_material(
            _device, AlphaMode::Opaque, matRes, _frames[0].frameDescriptorAllocator);

        // Create a new scene
        auto axisScene = std::make_shared<LoadedGLTF>();
        axisScene->creator = this;

        // Create a single MeshAsset for all axes
        auto meshAsset = std::make_shared<MeshAsset>();
        meshAsset->name = "Axes";
        GeoSurface surface;
        surface.startIndex = 0;
        surface.count = static_cast<uint32_t>(axisIndices.size());
        surface.bounds = bounds;
        surface.material = std::make_shared<MaterialInstance>(axisMaterial);
        meshAsset->surfaces.push_back(surface);
        meshAsset->meshBuffers = meshBuffers;

        // Create a single MeshNode for the axes
        auto meshNode = std::make_shared<MeshNode>();
        meshNode->mesh = meshAsset;
        meshNode->worldTransform = glm::mat4(1.0f);
        axisScene->topNodes.push_back(meshNode);

        _loadedScenes["axes"] = axisScene;
    }
}

void VulkanEngine::resize_swapchain()
{
    vkDeviceWaitIdle(_device);

    destroy_swapchain();

    int width, height;
    SDL_GetWindowSize(_window, &width, &height);
    _windowExtent.width = width;
    _windowExtent.height = height;

    create_swapchain(_windowExtent.width, _windowExtent.height);

    _resizeRequested = false;
}

void GLTFMetallicRoughness::build_pipelines(VulkanEngine *engine)
{
    VkShaderModule meshVertShader;
    if (!vkutil::load_shader_module(engine->_device, "shaders/mesh.vert.spv", &meshVertShader)) {
        throw std::runtime_error("Failed to load vertex shader module");
    }

    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module(engine->_device, "shaders/mesh.frag.spv", &meshFragShader)) {
        throw std::runtime_error("Failed to load fragment shader module");
    }

    VkShaderModule meshMaskedFragShader;
    if (!vkutil::load_shader_module(engine->_device, "shaders/mesh_masked.frag.spv", &meshMaskedFragShader)) {
        throw std::runtime_error("Failed to load masked fragment shader module");
    }

    VkPushConstantRange pushConstRange{};
    pushConstRange.offset = 0;
    pushConstRange.size = sizeof(MeshDrawPushConstants);
    pushConstRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptorSetLayout = layoutBuilder.build(engine->_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout setLayouts[] = {
        engine->_gpuSceneDataDescriptorSetLayout,
        descriptorSetLayout
    };

    VkPipelineLayoutCreateInfo meshLayoutInfo = vkinit::pipeline_layout_create_info();
    meshLayoutInfo.setLayoutCount = 2;
    meshLayoutInfo.pSetLayouts = setLayouts;
    meshLayoutInfo.pushConstantRangeCount = 1;
    meshLayoutInfo.pPushConstantRanges = &pushConstRange;
    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->_device, &meshLayoutInfo, nullptr, &newLayout));

    opaquePipeline.layout = transparentPipeline.layout = maskedPipeline.layout = newLayout;

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.set_shaders(meshVertShader, meshFragShader);
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.set_multisampling_none();
    pipelineBuilder.disable_blending();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    pipelineBuilder.set_color_attachment_format(engine->_drawImage.format);
    pipelineBuilder.set_depth_attachment_format(engine->_depthImage.format);
    pipelineBuilder._pipelineLayout = newLayout;

    opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

    pipelineBuilder.set_shaders(meshVertShader, meshMaskedFragShader);
    maskedPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    pipelineBuilder.enable_blending_additive();
    pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine->_device);

    vkDestroyShaderModule(engine->_device, meshVertShader, nullptr);
    vkDestroyShaderModule(engine->_device, meshFragShader, nullptr);
    vkDestroyShaderModule(engine->_device, meshMaskedFragShader, nullptr);
}

void GLTFMetallicRoughness::destroy_pipelines(VkDevice device)
{
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(device, opaquePipeline.layout, nullptr);
    // Shared layout so don't delete twice
    // TODO: Make this less hacky, add better ownership management
    //vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);
    vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
    vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
    vkDestroyPipeline(device, maskedPipeline.pipeline, nullptr);
}

MaterialInstance GLTFMetallicRoughness::write_material(VkDevice device, AlphaMode alphaMode, const MaterialResources& resources, DescriptorAllocatorGrowable descriptorAllocator)
{
    MaterialInstance matData;
    matData.alphaMode = alphaMode;
    if (alphaMode == AlphaMode::Transparent) {
        matData.pipeline = &transparentPipeline;
    } else if (alphaMode == AlphaMode::Masked) {
        matData.pipeline = &maskedPipeline;
    } else {
        matData.pipeline = &opaquePipeline;
    }

    matData.descriptorSet = descriptorAllocator.allocate(device, descriptorSetLayout);

    writer.clear();
    writer.write_buffer(0, resources.dataBuffer, sizeof(GLTFMetallicRoughness::MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write_image(1, resources.colorImage.view, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(2, resources.metalRoughImage.view, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update_set(device, matData.descriptorSet);

    return matData;
}

void MeshNode::gather_draw_data(const glm::mat4& topMatrix, DrawContext& ctx)
{
    glm::mat4 nodeMatrix = topMatrix * worldTransform;

    for (auto& s : mesh->surfaces) {
        RenderObject def;
        def.indexCount = s.count;
        def.firstIndex = s.startIndex;
        def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
        def.material = s.material.get(); // TODO: Weak pointer??
        def.bounds = s.bounds;
        def.transform = nodeMatrix;
        def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

        if (s.material->alphaMode == AlphaMode::Transparent) {
            ctx.TransparentDrawData.push_back(def);
        } else {
            ctx.OpaqueDrawData.push_back(def);
        }
    }

    // recurse down
    Node::gather_draw_data(nodeMatrix, ctx);
}