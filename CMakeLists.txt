# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.16)

# TODO: Rename this project
project ("vulkan_guide")

# GoogleTest FetchContent for unit tests
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


find_package(Vulkan REQUIRED)

add_subdirectory(third_party)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

# Set shaders output directory for all platforms/configurations
if(CMAKE_CONFIGURATION_TYPES)
    # Multi-config generator (e.g., Visual Studio)
    set(SHADERS_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/shaders")
else()
    # Single-config generator (e.g., Makefiles, Ninja)
    set(SHADERS_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
endif()

add_subdirectory(src)
enable_testing()
# Define a global BuildTests target before adding subdirectories
add_custom_target(BuildTests)
add_subdirectory(test)

# Examples (optional executables demonstrating engine & RHI usage)
if (EXISTS ${CMAKE_SOURCE_DIR}/examples/TeapotMeshlet.cpp)
  add_executable(TeapotMeshlet examples/TeapotMeshlet.cpp
    src/renderer/FrameManager.cpp
    src/resource/mesh/generator/MeshPrimitives.cpp
    src/resource/mesh/meshlet/MeshletBuilder.cpp
    src/rhi/vulkan/core/RHIVkContext.cpp
    src/rhi/vulkan/queue/RHIVkQueue.cpp
    src/rhi/vulkan/command/RHIVkCommandBuffer.cpp
    src/rhi/vulkan/swapchain/RHIVkSwapchain.cpp
    src/rhi/vulkan/buffer/RHIVkBuffer.cpp
    src/rhi/vulkan/image/RHIVkImage.cpp
    src/rhi/vulkan/image/RHIVkImageView.cpp
    src/rhi/vulkan/sync/RHIVkSemaphore.cpp
    src/rhi/vulkan/sync/RHIVkFence.cpp
    src/rhi/vulkan/core/RHIVkTypeConversion.cpp
    src/rhi/interface/image/RHIImageUtils.cpp
    src/rhi/vulkan/descriptor/RHIVkDescriptorSetLayout.cpp
    src/rhi/vulkan/descriptor/RHIVkDescriptorBuffer.cpp
    src/rhi/vulkan/descriptor/RHIVkDescriptorWriter.cpp
    src/rhi/vulkan/pipeline/RHIVkPipelineLayout.cpp
    src/rhi/vulkan/pipeline/RHIVkShaderModule.cpp
    src/rhi/vulkan/pipeline/RHIVkPipeline.cpp)
  set_property(TARGET TeapotMeshlet PROPERTY CXX_STANDARD 20)
  target_include_directories(TeapotMeshlet PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(TeapotMeshlet PRIVATE volk::volk Vulkan::Vulkan fmt::fmt SDL2::SDL2 SDL2::SDL2main glm unordered_dense svector VulkanMemoryAllocator)
  add_dependencies(TeapotMeshlet Shaders)
  if (WIN32)
    add_custom_command(TARGET TeapotMeshlet POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:TeapotMeshlet> $<TARGET_FILE_DIR:TeapotMeshlet>
      COMMAND_EXPAND_LISTS)
  endif()
endif()

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    "${PROJECT_SOURCE_DIR}/shaders/*.mesh"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  get_filename_component(FILE_EXT ${GLSL} LAST_EXT)
  set(SPIRV "${SHADERS_OUTPUT_DIRECTORY}/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  # All shaders target Vulkan 1.3; use list so args aren't concatenated into one token.
  set(EXTRA_GLSL_ARGS --target-env vulkan1.3)
  if (FILE_EXT STREQUAL ".mesh")
    list(APPEND EXTRA_GLSL_ARGS -S mesh)
  endif()
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_OUTPUT_DIRECTORY}
  COMMAND ${GLSL_VALIDATOR} ${EXTRA_GLSL_ARGS} -V ${GLSL} -o ${SPIRV}
  COMMAND ${CMAKE_COMMAND} -E echo "[GLSL] Built ${SPIRV}"
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

# --- Slang shader compilation (optional, if slangc is available) ---
find_program(SLANGC_EXECUTABLE NAMES slangc slangc.exe
  HINTS $ENV{VULKAN_SDK}/Bin $ENV{VULKAN_SDK}/Bin32 ${Vulkan_GLSLC_EXECUTABLE_PATH})

if(SLANGC_EXECUTABLE)
  file(GLOB_RECURSE SLANG_SOURCE_FILES "${PROJECT_SOURCE_DIR}/shaders/*.slang")
  foreach(SLANG_FILE ${SLANG_SOURCE_FILES})
    get_filename_component(SLANG_FILENAME ${SLANG_FILE} NAME)
    # Grab any @entry lines: // @entry <name> [profile] [target]
    file(STRINGS "${SLANG_FILE}" SLANG_ENTRY_LINES REGEX "^//[ \t]*@entry")
    if(SLANG_ENTRY_LINES)
      foreach(ENTRY_LINE ${SLANG_ENTRY_LINES})
        string(REGEX REPLACE "^//[ \t]*@entry[ \t]+" "" ENTRY_TAIL "${ENTRY_LINE}")
        separate_arguments(ENTRY_TOKENS NATIVE_COMMAND "${ENTRY_TAIL}")
        list(LENGTH ENTRY_TOKENS TOK_COUNT)
        if(TOK_COUNT GREATER 0)
          list(GET ENTRY_TOKENS 0 ENTRY_NAME)
          if(TOK_COUNT GREATER 1)
            list(GET ENTRY_TOKENS 1 ENTRY_PROFILE)
          else()
            set(ENTRY_PROFILE lib_6_5)
          endif()
          if(TOK_COUNT GREATER 2)
            list(GET ENTRY_TOKENS 2 ENTRY_TARGET)
          else()
            set(ENTRY_TARGET spirv)
          endif()
          set(OUT_FILE "${SHADERS_OUTPUT_DIRECTORY}/${SLANG_FILENAME}.spv")
          add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_OUTPUT_DIRECTORY}
            COMMAND ${SLANGC_EXECUTABLE} -target ${ENTRY_TARGET} -profile ${ENTRY_PROFILE} -fvk-use-scalar-layout -entry ${ENTRY_NAME} "${SLANG_FILE}" -o "${OUT_FILE}"
            COMMAND ${CMAKE_COMMAND} -E echo "[Slang] Built ${OUT_FILE}"
            DEPENDS ${SLANG_FILE}
            COMMENT "[Slang] @entry ${SLANG_FILENAME}:${ENTRY_NAME} (${ENTRY_PROFILE} -> ${ENTRY_TARGET})" VERBATIM)
          list(APPEND SPIRV_BINARY_FILES ${OUT_FILE})
        endif()
      endforeach()
    else()
      message(STATUS "failed to find @entry in ${SLANG_FILE}, skipping shader compilation")
    endif()
  endforeach()
else()
  message(STATUS "slangc not found; skipping Slang shader compilation")
endif()

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
)

# Custom target to build everything: engine, tests, shaders
add_custom_target(BuildAll
    DEPENDS engine Shaders BuildTests
)
