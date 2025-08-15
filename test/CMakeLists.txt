# Add test executable(s)


# Set test output directory for all platforms/configurations
if(CMAKE_CONFIGURATION_TYPES)
    set(TEST_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/test")
else()
    set(TEST_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test")
endif()

# # Vulkan-only test
# add_executable(compile_time_RHI_switch_vulkan experimental/compile_time_RHI_switch.cpp)
# set_property(TARGET compile_time_RHI_switch_vulkan PROPERTY CXX_STANDARD 20)
# set_target_properties(compile_time_RHI_switch_vulkan PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
# target_include_directories(compile_time_RHI_switch_vulkan PRIVATE "${CMAKE_SOURCE_DIR}/src")
# target_link_libraries(compile_time_RHI_switch_vulkan PRIVATE GTest::gtest GTest::gtest_main vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
# target_compile_definitions(compile_time_RHI_switch_vulkan PRIVATE USE_VULKAN_RHI=1)
# add_dependencies(BuildTests compile_time_RHI_switch_vulkan)
# add_test(NAME compile_time_RHI_switch_vulkan COMMAND compile_time_RHI_switch_vulkan)

# # Metal-only test
# add_executable(compile_time_RHI_switch_metal experimental/compile_time_RHI_switch.cpp)
# set_property(TARGET compile_time_RHI_switch_metal PROPERTY CXX_STANDARD 20)
# set_target_properties(compile_time_RHI_switch_metal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
# target_include_directories(compile_time_RHI_switch_metal PRIVATE "${CMAKE_SOURCE_DIR}/src")
# target_link_libraries(compile_time_RHI_switch_metal PRIVATE GTest::gtest GTest::gtest_main vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
# target_compile_definitions(compile_time_RHI_switch_metal PRIVATE USE_METAL_RHI=1)
# add_dependencies(BuildTests compile_time_RHI_switch_metal)
# add_test(NAME compile_time_RHI_switch_metal COMMAND compile_time_RHI_switch_metal)

# # Mock-only test
# add_executable(compile_time_RHI_switch_mock experimental/compile_time_RHI_switch.cpp)
# set_property(TARGET compile_time_RHI_switch_mock PROPERTY CXX_STANDARD 20)
# set_target_properties(compile_time_RHI_switch_mock PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
# target_include_directories(compile_time_RHI_switch_mock PRIVATE "${CMAKE_SOURCE_DIR}/src")
# target_link_libraries(compile_time_RHI_switch_mock PRIVATE GTest::gtest GTest::gtest_main vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
# target_compile_definitions(compile_time_RHI_switch_mock PRIVATE USE_MOCK_RHI=1)
# add_dependencies(BuildTests compile_time_RHI_switch_mock)
# add_test(NAME compile_time_RHI_switch_mock COMMAND compile_time_RHI_switch_mock)

# # Virtual RHI (multi-backend) test
# add_executable(compile_time_RHI_switch_virtual experimental/compile_time_RHI_switch.cpp)
# set_property(TARGET compile_time_RHI_switch_virtual PROPERTY CXX_STANDARD 20)
# set_target_properties(compile_time_RHI_switch_virtual PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
# target_include_directories(compile_time_RHI_switch_virtual PRIVATE "${CMAKE_SOURCE_DIR}/src")
# target_link_libraries(compile_time_RHI_switch_virtual PRIVATE GTest::gtest GTest::gtest_main vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
# target_compile_definitions(compile_time_RHI_switch_virtual PRIVATE USE_VULKAN_RHI=1 USE_METAL_RHI=1)
# add_dependencies(BuildTests compile_time_RHI_switch_virtual)
# add_test(NAME compile_time_RHI_switch_virtual COMMAND compile_time_RHI_switch_virtual)

# Struct pass performance test
add_executable(struct_pass_perf_test experimental/struct_pass_perf_test.cpp)
set_property(TARGET struct_pass_perf_test PROPERTY CXX_STANDARD 20)
set_target_properties(struct_pass_perf_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(struct_pass_perf_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
add_dependencies(BuildTests struct_pass_perf_test)
add_test(NAME struct_pass_perf_test COMMAND struct_pass_perf_test)

# Slotmap unit test
add_executable(SlotMapTest SlotMapTest.cpp)
set_property(TARGET SlotMapTest PROPERTY CXX_STANDARD 20)
set_target_properties(SlotMapTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(SlotMapTest PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(SlotMapTest PRIVATE GTest::gtest GTest::gtest_main unordered_dense svector)
add_dependencies(BuildTests SlotMapTest)
add_test(NAME SlotMapTest COMMAND SlotMapTest)

# Scene unit test
add_executable(SceneTest SceneTest.cpp ${CMAKE_SOURCE_DIR}/src/scene/Scene.cpp)
set_property(TARGET SceneTest PROPERTY CXX_STANDARD 20)
set_target_properties(SceneTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(SceneTest PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(SceneTest PRIVATE GTest::gtest GTest::gtest_main glm unordered_dense svector)
add_dependencies(BuildTests SceneTest)
add_test(NAME SceneTest COMMAND SceneTest)

# SceneManager unit test
add_executable(SceneManagerTest 
    SceneManagerTest.cpp 
    ${CMAKE_SOURCE_DIR}/src/scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/src/scene/SceneManager.cpp)
set_property(TARGET SceneManagerTest PROPERTY CXX_STANDARD 20)
set_target_properties(SceneManagerTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(SceneManagerTest PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(SceneManagerTest PRIVATE GTest::gtest GTest::gtest_main glm unordered_dense svector)
add_dependencies(BuildTests SceneManagerTest)
add_test(NAME SceneManagerTest COMMAND SceneManagerTest)

# RHI Vulkan test
add_executable(RHIVulkanTest
    RHIVulkanTest.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/FrameManager.cpp
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
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/descriptor/RHIVkDescriptorSetLayoutBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/descriptor/RHIVkDescriptorBuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/descriptor/RHIVkDescriptorWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/pipeline/RHIVkPipelineLayoutBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/pipeline/RHIVkPipelineLayout.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/pipeline/RHIVkShaderModule.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/pipeline/RHIVkPipeline.cpp
)
set_property(TARGET RHIVulkanTest PROPERTY CXX_STANDARD 20)
set_target_properties(RHIVulkanTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(RHIVulkanTest PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(RHIVulkanTest PRIVATE volk::volk GTest::gtest GTest::gtest_main Vulkan::Vulkan fmt::fmt SDL2::SDL2 glm unordered_dense svector VulkanMemoryAllocator)
if (WIN32)
  add_custom_command(TARGET RHIVulkanTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:RHIVulkanTest> $<TARGET_FILE_DIR:RHIVulkanTest>
    COMMAND_EXPAND_LISTS
  )
endif()
add_dependencies(BuildTests RHIVulkanTest)
add_test(NAME RHIVulkanTest COMMAND RHIVulkanTest)
