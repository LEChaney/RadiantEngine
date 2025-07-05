# RadiantEngine High-Level Design Overview

This document provides a high-level architectural overview of RadiantEngine, summarizing the core systems, their responsibilities, and how they interact. For detailed design and API documentation, see the linked module documents.

---

## 1. Architecture Summary

RadiantEngine is a modular, testable, and data-driven real-time rendering engine. Its architecture emphasizes clear separation of concerns, robust resource management, and modern rendering techniques (instancing, culling, GPU-driven workflows, etc.).

Key principles:
- **Modularity:** Each system (renderer, scene, draw data, culling, resource management, etc.) is encapsulated and interacts via explicit APIs and observer patterns.
- **Testability:** All dependencies are injected or registered, supporting unit and integration testing.
- **Data-Oriented Design:** Flat, cache-friendly data layouts and slot map containers are used for all dynamic data.
- **Robust Resource Ownership:** Ownership and lifetime of all resources (CPU and GPU) are clearly defined and documented.

---

## 2. Core Systems and Documentation

### Renderer
- Orchestrates the rendering pipeline, manages the main loop, and coordinates draw calls, culling, and resource binding.
- Handles Vulkan initialization, swapchain setup, and frame lifecycle.
- See: [renderer.md](renderer/renderer.md)

### Draw Data Manager
- Owns and manages all per-draw and per-instance data (draw groups, instance data, culling, bounds, etc.) using slot maps.
- Ensures robust synchronization and efficient updates via observer pattern and reverse handle lookups.
- See: [drawdata.md](renderer/drawdata.md)

### Mesh System
- Manages mesh resources per scene, including GPU buffers and mesh/section associations.
- Integrates with DrawDataManager for draw data generation and supports automatic instancing.
- See: [mesh_system.md](renderer/mesh_system.md)

### Culling System
- Stateless, efficient culling of draw data based on camera frustum and per-draw bounds.
- Consumes data from DrawDataManager and returns visible draw handles for rendering.
- See: [culling_system.md](renderer/culling_system.md)

### Scene System
- Manages scene graphs, nodes, transforms, and resource associations.
- Notifies observers (e.g., DrawDataManager) of changes for efficient data updates.
- See: [scene.md](scene/scene.md)

### Resource Management
- Handles allocation and deallocation of GPU resources (buffers, images, samplers) and CPU-side assets.
- See: [scene_resource_ownership.md](renderer/scene_resource_ownership.md)

### Slot Map Containers
- All dynamic data (draw groups, instances, culling, bounds, lights) is managed using slot maps for stable handles and robust associations.
- See: [slotmap.md](utils/containers/slotmap.md)

---

## 3. System Integration and Data Flow

- **Scene changes** trigger notifications to the renderer, which clears and repopulates draw and culling data.
- **DrawDataManager** observes mesh, material, and transform changes, updating all per-draw and per-instance data.
- **CullingSystem** queries DrawDataManager for bounds and instance data, returning visible draws for the current frame.
- **Renderer** issues draw calls using the visible draw handles and manages the frame lifecycle, swapchain, and presentation.
- **ResourceManager** and **PipelineManager** provide allocation and state management for all GPU resources and pipelines.

---

## 4. References and Further Reading

- [Renderer Module](renderer/renderer.md)
- [Draw Data Manager](renderer/drawdata.md)
- [Mesh System](renderer/mesh_system.md)
- [Culling System](renderer/culling_system.md)
- [Scene System](scene/scene.md)
- [Resource Ownership](renderer/scene_resource_ownership.md)
- [Slot Map Container](utils/containers/slotmap.md)

For detailed API, data structure, and workflow documentation, see the linked module documents above.

---

This document should be updated as the engine architecture evolves.
