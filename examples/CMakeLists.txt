# Examples CMakeLists
# Each example links directly against engine implementation units (temporary until engine lib target exists)

set(EXAMPLE_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/renderer/FrameManager.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/mesh/generator/MeshPrimitives.cpp
    ${CMAKE_SOURCE_DIR}/src/resource/mesh/meshlet/MeshletBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/core/RHIVkContext.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/queue/RHIVkQueue.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/command/RHIVkCommandBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/swapchain/RHIVkSwapchain.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/buffer/RHIVkBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/image/RHIVkImage.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/image/RHIVkImageView.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/sync/RHIVkSemaphore.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/sync/RHIVkFence.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/core/RHIVkTypeConversion.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/interface/image/RHIImageUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/descriptor/RHIVkDescriptorBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/descriptor/RHIVkDescriptorWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/pipeline/RHIVkPipelineLayout.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/pipeline/RHIVkShaderModule.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/pipeline/RHIVkPipeline.cpp
)

function(add_radiant_example target source)
    add_executable(${target} ${source} ${EXAMPLE_COMMON_SOURCES})
    set_property(TARGET ${target} PROPERTY CXX_STANDARD 20)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${target} PRIVATE volk::volk Vulkan::Vulkan fmt::fmt SDL2::SDL2 SDL2::SDL2main glm unordered_dense svector VulkanMemoryAllocator)
    add_dependencies(${target} Shaders)
    if (WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS)
    endif()
endfunction()

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/TeapotMeshlet.cpp)
    add_radiant_example(TeapotMeshlet ${CMAKE_CURRENT_SOURCE_DIR}/TeapotMeshlet.cpp)
endif()

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/DrawAxes.cpp)
    add_radiant_example(DrawAxes ${CMAKE_CURRENT_SOURCE_DIR}/DrawAxes.cpp)
endif()

# New multi-mesh task shader stress test example
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/MultiMeshTask.cpp)
    add_radiant_example(MultiMeshTask ${CMAKE_CURRENT_SOURCE_DIR}/MultiMeshTask.cpp)
endif()
