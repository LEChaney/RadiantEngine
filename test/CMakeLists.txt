# Add test executable(s)


# Set test output directory for all platforms/configurations
if(CMAKE_CONFIGURATION_TYPES)
    set(TEST_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/test")
else()
    set(TEST_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test")
endif()

# Vulkan-only test
add_executable(compile_time_RHI_switch_vulkan experimental/compile_time_RHI_switch.cpp)
set_property(TARGET compile_time_RHI_switch_vulkan PROPERTY CXX_STANDARD 20)
set_target_properties(compile_time_RHI_switch_vulkan PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(compile_time_RHI_switch_vulkan PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(compile_time_RHI_switch_vulkan PRIVATE vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
target_compile_definitions(compile_time_RHI_switch_vulkan PRIVATE USE_VULKAN_RHI=1)
add_dependencies(build_tests compile_time_RHI_switch_vulkan)
add_test(NAME compile_time_RHI_switch_vulkan COMMAND compile_time_RHI_switch_vulkan)

# Metal-only test
add_executable(compile_time_RHI_switch_metal experimental/compile_time_RHI_switch.cpp)
set_property(TARGET compile_time_RHI_switch_metal PROPERTY CXX_STANDARD 20)
set_target_properties(compile_time_RHI_switch_metal PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(compile_time_RHI_switch_metal PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(compile_time_RHI_switch_metal PRIVATE vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
target_compile_definitions(compile_time_RHI_switch_metal PRIVATE USE_METAL_RHI=1)
add_dependencies(build_tests compile_time_RHI_switch_metal)
add_test(NAME compile_time_RHI_switch_metal COMMAND compile_time_RHI_switch_metal)

# Mock-only test
add_executable(compile_time_RHI_switch_mock experimental/compile_time_RHI_switch.cpp)
set_property(TARGET compile_time_RHI_switch_mock PROPERTY CXX_STANDARD 20)
set_target_properties(compile_time_RHI_switch_mock PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(compile_time_RHI_switch_mock PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(compile_time_RHI_switch_mock PRIVATE vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
target_compile_definitions(compile_time_RHI_switch_mock PRIVATE USE_MOCK_RHI=1)
add_dependencies(build_tests compile_time_RHI_switch_mock)
add_test(NAME compile_time_RHI_switch_mock COMMAND compile_time_RHI_switch_mock)

# Virtual RHI (multi-backend) test
add_executable(compile_time_RHI_switch_virtual experimental/compile_time_RHI_switch.cpp)
set_property(TARGET compile_time_RHI_switch_virtual PROPERTY CXX_STANDARD 20)
set_target_properties(compile_time_RHI_switch_virtual PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(compile_time_RHI_switch_virtual PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(compile_time_RHI_switch_virtual PRIVATE vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf)
target_compile_definitions(compile_time_RHI_switch_virtual PRIVATE USE_VULKAN_RHI=1 USE_METAL_RHI=1)
add_dependencies(build_tests compile_time_RHI_switch_virtual)
add_test(NAME compile_time_RHI_switch_virtual COMMAND compile_time_RHI_switch_virtual)

# Slotmap unit test
add_executable(slotmap_test slotmap_test.cpp)
set_property(TARGET slotmap_test PROPERTY CXX_STANDARD 20)
set_target_properties(slotmap_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIRECTORY}")
target_include_directories(slotmap_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(slotmap_test PRIVATE GTest::gtest GTest::gtest_main)
add_dependencies(build_tests slotmap_test)
add_test(NAME slotmap_test COMMAND slotmap_test)
