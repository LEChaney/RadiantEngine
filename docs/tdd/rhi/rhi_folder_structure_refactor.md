## RHI Folder Structure Refactor Suggestions

To improve maintainability and clarity in the `src/rhi` and `src/rhi/vulkan` directories, consider the following reorganization strategies:

### 1. Group by Resource Type

Create subfolders for major resource types. For example:

```
src/rhi/
  buffer/
    rhi_buffer.h
    rhi_buffer.cpp
  image/
    rhi_image.h
    rhi_image.cpp
    rhi_image_view.h
    rhi_image_view.cpp
    rhi_image_utils.h
    rhi_image_utils.cpp
  pipeline/
    rhi_pipeline.h
  descriptor/
    rhi_descriptor_set.h
    rhi_descriptor_set_writer.h
  command/
    rhi_command_buffer.h
  sync/
    rhi_fence.h
  ...
```

And similarly for Vulkan:

```
src/rhi/vulkan/
  buffer/
    rhivk_buffer.h
    rhivk_buffer.cpp
  image/
    rhivk_image.h
    rhivk_image.cpp
    rhivk_image_view.h
    rhivk_image_view.cpp
  pipeline/
    rhivk_pipeline.h
  descriptor/
    rhivk_descriptor_set_writer.h
    rhivk_descriptor_set_writer.cpp
  command/
    rhivk_command_buffer.h
    rhivk_command_buffer.cpp
  sync/
    rhivk_fence.h
    rhivk_fence.cpp
    rhivk_semaphore.h
    rhivk_semaphore.cpp
  swapchain/
    rhivk_swapchain.h
    rhivk_swapchain.cpp
  ...
```

### 2. Group by Functionality

Alternatively, group files by their role in the engine:

- **Core**: Definitions, enums, and utility functions.
- **Resources**: Buffers, images, pipelines.
- **Execution**: Command buffers, queues.
- **Synchronization**: Fences, semaphores.
- **Descriptors**: Descriptor sets and writers.

### 3. Use Namespaces and CMake Targets

If you use CMake, create targets for each group to manage dependencies and compilation more easily.

### 4. Prefixes/Suffixes

If you prefer not to use folders, use consistent prefixes or suffixes (e.g., `rhi_buffer_*`, `rhivk_buffer_*`) to group related files.

---

**Summary:**
Grouping by resource type or functionality into subfolders will make your codebase easier to navigate and maintain. This also helps new contributors quickly find relevant files.
