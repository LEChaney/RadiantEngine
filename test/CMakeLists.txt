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
# add_dependencies(build_tests compile_time_RHI_switch_vulkan)
# add_test(NAME compile_time_RHI_switch_vulkan COMMAND compile_time_RHI_switch_vulkan)

# # Metal-only test
# add_executable(compile_time_RHI_switch_metal experimental/compile_time_RHI_switch.cpp)
# set_property(TARGET compile_time_RHI_switch_metal PROPERTY CXX_STANDARD 20)
# set_target_properties(compile_time_RHI_switch_metal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
# target_include_directories(compile_time_RHI_switch_metal PRIVATE "${CMAKE_SOURCE_DIR}/src")
# target_link_libraries(compile_time_RHI_switch_metal PRIVATE GTest::gtest GTest::gtest_main vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
# target_compile_definitions(compile_time_RHI_switch_metal PRIVATE USE_METAL_RHI=1)
# add_dependencies(build_tests compile_time_RHI_switch_metal)
# add_test(NAME compile_time_RHI_switch_metal COMMAND compile_time_RHI_switch_metal)

# # Mock-only test
# add_executable(compile_time_RHI_switch_mock experimental/compile_time_RHI_switch.cpp)
# set_property(TARGET compile_time_RHI_switch_mock PROPERTY CXX_STANDARD 20)
# set_target_properties(compile_time_RHI_switch_mock PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
# target_include_directories(compile_time_RHI_switch_mock PRIVATE "${CMAKE_SOURCE_DIR}/src")
# target_link_libraries(compile_time_RHI_switch_mock PRIVATE GTest::gtest GTest::gtest_main vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
# target_compile_definitions(compile_time_RHI_switch_mock PRIVATE USE_MOCK_RHI=1)
# add_dependencies(build_tests compile_time_RHI_switch_mock)
# add_test(NAME compile_time_RHI_switch_mock COMMAND compile_time_RHI_switch_mock)

# # Virtual RHI (multi-backend) test
# add_executable(compile_time_RHI_switch_virtual experimental/compile_time_RHI_switch.cpp)
# set_property(TARGET compile_time_RHI_switch_virtual PROPERTY CXX_STANDARD 20)
# set_target_properties(compile_time_RHI_switch_virtual PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
# target_include_directories(compile_time_RHI_switch_virtual PRIVATE "${CMAKE_SOURCE_DIR}/src")
# target_link_libraries(compile_time_RHI_switch_virtual PRIVATE GTest::gtest GTest::gtest_main vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
# target_compile_definitions(compile_time_RHI_switch_virtual PRIVATE USE_VULKAN_RHI=1 USE_METAL_RHI=1)
# add_dependencies(build_tests compile_time_RHI_switch_virtual)
# add_test(NAME compile_time_RHI_switch_virtual COMMAND compile_time_RHI_switch_virtual)

# Struct pass performance test
add_executable(struct_pass_perf_test experimental/struct_pass_perf_test.cpp)
set_property(TARGET struct_pass_perf_test PROPERTY CXX_STANDARD 20)
set_target_properties(struct_pass_perf_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(struct_pass_perf_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
add_dependencies(build_tests struct_pass_perf_test)
add_test(NAME struct_pass_perf_test COMMAND struct_pass_perf_test)

# Slotmap unit test
add_executable(slotmap_test slotmap_test.cpp)
set_property(TARGET slotmap_test PROPERTY CXX_STANDARD 20)
set_target_properties(slotmap_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(slotmap_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(slotmap_test PRIVATE GTest::gtest GTest::gtest_main unordered_dense)
add_dependencies(build_tests slotmap_test)
add_test(NAME slotmap_test COMMAND slotmap_test)

# Scene unit test
add_executable(scene_test scene_test.cpp ${CMAKE_SOURCE_DIR}/src/scene/scene.cpp)
set_property(TARGET scene_test PROPERTY CXX_STANDARD 20)
set_target_properties(scene_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(scene_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(scene_test PRIVATE GTest::gtest GTest::gtest_main glm unordered_dense)
add_dependencies(build_tests scene_test)
add_test(NAME scene_test COMMAND scene_test)

# SceneManager unit test
add_executable(scene_manager_test 
    scene_manager_test.cpp 
    ${CMAKE_SOURCE_DIR}/src/scene/scene.cpp
    ${CMAKE_SOURCE_DIR}/src/scene/scene_manager.cpp)
set_property(TARGET scene_manager_test PROPERTY CXX_STANDARD 20)
set_target_properties(scene_manager_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(scene_manager_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(scene_manager_test PRIVATE GTest::gtest GTest::gtest_main glm unordered_dense)
add_dependencies(build_tests scene_manager_test)
add_test(NAME scene_manager_test COMMAND scene_manager_test)

# RHI Vulkan test
add_executable(rhi_vulkan_test
    rhi_vulkan_test.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/rhivk_context.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/rhivk_queue.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/rhivk_command_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/rhivk_swapchain.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/rhivk_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/rhivk_image.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/rhivk_image_view.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/rhivk_semaphore.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/rhivk_fence.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/vulkan/rhivk_core_defs.cpp
    ${CMAKE_SOURCE_DIR}/src/rhi/rhi_image_utils.cpp
)
set_property(TARGET rhi_vulkan_test PROPERTY CXX_STANDARD 20)
set_target_properties(rhi_vulkan_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(rhi_vulkan_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(rhi_vulkan_test PRIVATE GTest::gtest GTest::gtest_main Vulkan::Vulkan fmt::fmt SDL2::SDL2 glm unordered_dense VulkanMemoryAllocator)
if (WIN32)
  add_custom_command(TARGET rhi_vulkan_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:rhi_vulkan_test> $<TARGET_FILE_DIR:rhi_vulkan_test>
    COMMAND_EXPAND_LISTS
  )
endif()
add_dependencies(build_tests rhi_vulkan_test)
add_test(NAME rhi_vulkan_test COMMAND rhi_vulkan_test)
