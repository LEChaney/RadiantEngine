# Scene Module Design Document

## 1. Purpose

The scene module is responsible for representing and managing the logical structure and transform hierarchy of the virtual world on the CPU side. It provides a minimal, flat scene graph for hierarchical transforms, and serves as the central point of association for all per-system data (rendering, lighting, physics, etc.).

---

## 2. Design Principles

- **Minimalism:** The scene graph only manages hierarchy and transforms. No node types, no polymorphism, no component system.
- **Slot Map Storage:** All nodes are stored in a slot map, referenced by a `NodeHandle` (a slot map key). Scenes themselves are also managed in a slot map, referenced by a `SceneHandle`.
- **Single Scene Root Node:** Each scene contains a single, always-present root node (`NodeHandle sceneRoot`). All user nodes are descendants of this root. The scene root can be used to move, scale, or rotate the entire scene as a unit, and simplifies global transforms and scene instancing.
- **Separation of Data:** All system-specific data (meshes, lights, physics, etc.) is stored externally, associated to nodes via `NodeHandle`.
- **Explicit Association:** Systems maintain their own data arrays, each entry referencing a node. The scene graph is agnostic to what data is attached to each node.
- **Cache-Friendly:** Slot map storage and separation of data enable efficient traversal, stable handles, and system updates.

---

## 3. Scene Graph Structure

### Node (internal structure)
- `NodeHandle parent` (slot map key)
- `std::vector<NodeHandle> children` (slot map keys)
- `glm::mat4 localTransform`
- `glm::mat4 worldTransform`
- `std::string name`
- `bool dirty` // **True if this node (or its ancestors) need world transform update**

All nodes are stored in a slot map, indexed by `NodeHandle` (a slot map key, see [Slot Map Container](../utils/containers/slotmap.md)).

#### Scene Root Node
- Each scene contains a single root node, accessible via `scene.getRootNode()` or `scene.sceneRoot`.
- The root node is always present and cannot be removed.
- All user nodes are added as children (directly or indirectly) of the root node.
- Modifying the root node's transform moves, rotates, or scales the entire scene as a unit.
- The root node's parent is always `INVALID_HANDLE`.

---

## 4. Public API

### Scene
- `NodeHandle getRootNode() const` – Returns the handle to the scene's root node.
- `NodeHandle addNode(NodeHandle parent = INVALID_HANDLE)` – Create a new node, optionally as a child of `parent`. If `parent` is `INVALID_HANDLE`, the node is added as a child of the scene root. Returns a slot map key.
- `void removeNode(NodeHandle)` – Remove a node and its descendants.
- `NodeHandle getParent(NodeHandle)` / `std::vector<NodeHandle> getChildren(NodeHandle)` – Query parent/children.
- `void traverse(std::function<void(NodeHandle)>, NodeHandle start = INVALID_HANDLE)` – Traverse the hierarchy, visiting each node by handle, starting at `start` (default: all roots).
- `NodeHandle findNodeByName(const std::string&)` – Find a node by name.
- `glm::mat4 getLocalTransform(NodeHandle)` – Get local transform for a node.
- `glm::mat4 getWorldTransform(NodeHandle node, bool updateIfDirty = true)` – Returns the world transform for a node. If `updateIfDirty` is true (default), will immediately propagate transforms for the node (and its ancestors) if dirty, ensuring the returned transform is always up to date. If false, returns the cached world transform (may be stale if not synchronized).
- `void setLocalTransform(NodeHandle, const glm::mat4&)` – Set local transform for a node. **Marks the node and all descendants as dirty for transform propagation.**
- `void setWorldTransform(NodeHandle node, const glm::mat4& world)` – Sets the node's world transform by computing and setting the appropriate local transform such that `parentWorld * local = world`. This function always propagates transforms up to the parent (if dirty) before computing the local transform, ensuring correctness. The node and its descendants are then marked dirty for propagation.
- `void propagateTransformsTo(NodeHandle target)` – Propagates transforms from the nearest dirty ancestor (or itself) down to the specified target node, updating only the necessary chain. Stops as soon as the target node is up to date and no longer dirty. The minimal dirty set is updated accordingly to remove any nodes that are no longer dirty as a result of this targeted propagation. This enables efficient, granular updates for on-demand queries or partial scene updates.
- `void propogateTransforms()` – Propagate local-to-world transforms for all dirty nodes and their descendants. **Clears the dirty set and updates the changed nodes set.**
  - **Note:** As of the latest architecture, you should not call `propogateTransforms()` independently per frame. Instead, call `finalizeForRendering()`, which performs both transform propagation and observer notification in a single step.
- `void finalizeForRendering()` – Performs final transform propagation (calls `propogateTransforms()` internally if needed), notifies all registered observers (e.g., CullingSystem, DrawDataManager) of node transform changes via the observer pattern, passing the set of changed nodes, and then clears the changed nodes set. This should be called once per frame, after all transform updates and before rendering, to ensure all systems are synchronized efficiently.
- `const std::unordered_set<NodeHandle>& getChangedNodes() const` – Returns a const reference to the set of nodes whose world transforms changed during all propagations since the last explicit clear. **This set is only cleared by the user.**
- `void clearChangedNodes()` – Clears the set of changed nodes.
- `bool isNodeDirty(NodeHandle node) const` – Returns true if the node is dirty. This is useful for checking whether a node's world transform is up to date.
- `void loadFromFile(path)` / `void saveToFile(path)` – Load/save scene graph and associations.
  - *Note: These functions will select an appropriate loader or saver implementation under the hood (e.g., the GLTF scene loader for `.gltf`/`.glb` files). Initially, only the GLTF loader will be implemented. Saving is not planned for the initial release.*

---

## 5. System Data (External to Scene Graph)
Each system (rendering, lighting, physics, etc.) is responsible for managing its own data structures, with each data entry explicitly associated with a node via its `NodeHandle`. The scene graph itself is agnostic to what data is attached to each node; all associations are maintained externally by the systems.

**Association Pattern:**
- Systems typically store their data in arrays, slot maps, or hash maps, where each entry includes a `NodeHandle` to indicate which node it is attached to.
- To associate a mesh instance with a node, the rendering system might store:
    ```cpp
    struct MeshInstance {
            NodeHandle node;    // The node this mesh instance is attached to
            Mesh* meshAsset;    // Pointer or handle to mesh asset
            Material* material; // Pointer or handle to material
            // ...other per-instance data...
    };
    std::vector<MeshInstance> meshInstances;
    ```
- Similarly, the lighting system might use:
    ```cpp
    struct LightInstance {
            NodeHandle node;    // The node this light is attached to
            LightParams params; // Light parameters (color, intensity, etc.)
            // ...other per-light data...
    };
    std::vector<LightInstance> lightInstances;
    ```

**Lookup and Synchronization:**
- To find all mesh instances for rendering, iterate the `meshInstances` array and use each entry's `node` to query the node's world transform from the scene.
- To find all lights, iterate the `lightInstances` array in the same way.
- If fast lookup from `NodeHandle` to system data is needed (e.g., for removal or updates), systems may maintain an auxiliary map:
    ```cpp
    std::unordered_map<NodeHandle, size_t> nodeToMeshInstanceIndex;
    ```
- When a node is removed from the scene, each system is responsible for removing or updating any associated data entries.

**Summary:**  
- The association between nodes and system data is always explicit and external to the scene graph.
- The scene graph only provides stable `NodeHandle` keys and transform queries; all other data management is handled by the systems themselves.
- This design enables flexible, decoupled, and cache-friendly updates across all systems.

---

## 6. Association and Traversal

- The scene graph is responsible for transform propagation and hierarchy only.
- Systems are responsible for associating their data to nodes and updating as needed.
- To find all mesh data, iterate the mesh data array; to find all lights, iterate the light data array, etc.
- To find the node for a given data entry, use the stored `NodeHandle` (slot map key).
- **Transform Synchronization:** After all transform updates, call `finalizeForRendering()` to perform final transform propagation and notify registered observers (such as CullingSystem, DrawDataManager, etc.) via the observer pattern, passing the set of changed nodes for synchronization. This ensures all systems are updated exactly once per frame, just before rendering. See [Observer Pattern](../core/observer_pattern.md) for details.

---

## 7. SceneManager

- **SceneManager is responsible for managing all loaded scenes and determining which scene(s) are active.**
- Systems such as the Renderer, DrawDataManager, and CullingSystem should register as observers to the SceneManager to be notified when the set of active scenes changes. This enables them to repopulate or clear their data as needed.
- The SceneManager enforces invariants (e.g., only one active scene, or a set of active scenes for multi-viewport/multi-world support) and decouples scene activation from rendering logic.

### API
- `SceneHandle addScene()` – Adds a new scene to the manager, returns a slot map key.
- `void removeScene(SceneHandle)` – Removes a scene by handle.
- `Scene* getScene(SceneHandle)` – Retrieves a scene by handle.
- `Scene* getActiveScene() const` – Returns the currently active scene (if only one is supported).
- `std::vector<SceneHandle> getActiveScenes() const` – Returns all currently active scenes (for multi-scene support).
- `void setActiveScene(SceneHandle)` – Sets the active scene by handle.
- `void setActiveScenes(const std::vector<SceneHandle>& scenes)` – Sets the set of active scenes (for multi-scene support).
- `void addObserver(ISceneManagerObserver* observer)` – Register an observer to be notified when the active scene(s) change.
- `void removeObserver(ISceneManagerObserver* observer)` – Unregister an observer.

#### Observer Interface Example
```cpp
class ISceneManagerObserver {
public:
    virtual void onActiveScenesChanged(const std::vector<SceneHandle>& newActiveScenes) = 0;
};
```

#### Workflow
- When the active scene(s) change, the SceneManager notifies all registered observers via `onActiveScenesChanged`.
- Observers (Renderer, DrawDataManager, etc.) then clear and repopulate their data from the new active scene(s).
- This pattern supports both single-scene and multi-scene rendering, and enables robust, decoupled system updates.

Scenes are stored in a slot map, indexed by `SceneHandle` (a slot map key).

---

## 8. Transform Propagation, Dirtying, and Synchronization (Logic & Pseudocode)

### 8.1 Dirty Flag Propagation and Minimal Dirty Set

- When a node's local transform is changed via `setLocalTransform`, that node **and all of its descendants** have their `dirty` flag immediately set to true. This ensures that checking whether a node's world transform is up to date is a simple matter of checking its own `dirty` flag.
- To ensure efficient propagation, the minimal dirty set algorithm is used to maintain only the minimal set of dirty roots. When marking a node dirty, if its parent is already dirty, no further action is needed (the parent's propagation will cover this node). Otherwise, the node is added to the dirty set, and all descendants are removed from the dirty set (since they will be covered by this node's propagation).

### 8.2 World Transform Propagation

- The method `propogateTransforms()` traverses the scene graph, propagating world transforms from parents to children for all dirty roots and their descendants. It updates the world transform for each affected node and clears their `dirty` flag.
- The method `propagateTransformsTo(NodeHandle target)` propagates transforms from the nearest dirty ancestor (or itself) down to the specified target node, updating only the necessary chain. This is useful for on-demand updates of a single node or subtree, and is used internally by methods like `getWorldTransform(node, true)` and `setWorldTransform`.
- After propagation, the minimal dirty set is updated to remove any nodes that are no longer dirty as a result of this targeted update.
- After `propogateTransforms()` completes, all nodes are removed from the dirty set and their `dirty` flags are cleared. The dirty state is now clear, and the scene is considered up to date until the next call to `setLocalTransform`.
- **Note:** `finalizeForRendering()` should be used to perform final transform propogation and notify observers once per frame. This ensures all systems are synchronized efficiently before rendering.

### 8.3 Changed Nodes Tracking

- As each node's world transform is updated, its handle is added to a `changedNodes` set. This set records all nodes whose world transforms were modified during the last propagation(s). The set is only cleared when the user explicitly calls `clearChangedNodes()`.
- The set persists and accumulates across multiple calls to `propogateTransforms()` until the user explicitly clears it. This allows for multiple transform propagations within a frame if needed (e.g., for dependency updates), without losing track of which nodes have changed.

### 8.4 Rationale

By immediately setting the `dirty` flag on a node and all its descendants when `setLocalTransform` is called, checking whether a node's world transform is up to date is a simple, fast operation: just check the node's own `dirty` flag. The minimal dirty set ensures that each affected transform chain is only updated once, regardless of the order or number of `setLocalTransform` calls, and avoids redundant updates even in complex scenarios.

### 8.5 Implementation Pseudocode

```cpp
void setLocalTransform(NodeHandle node, const glm::mat4& local) {
    node.localTransform = local;
    markDirty(node);
}

void markDirty(NodeHandle node) {
    // 1. If parent is dirty, do nothing
    if (node.parent != INVALID_HANDLE && nodes[node.parent].dirty) {
        return;
    }
    // 2. Add node to dirty set
    dirtySet.insert(node);
    // 3. Remove all descendants from dirty set
    for (NodeHandle desc : getDescendants(node)) {
        dirtySet.erase(desc);
    }
    // 4. Set dirty flag on node and all descendants
    setDirtyFlagRecursive(node);
}

void setDirtyFlagRecursive(NodeHandle node) {
    nodes[node].dirty = true;
    for (NodeHandle child : nodes[node].children) {
        setDirtyFlagRecursive(child);
    }
}

void propogateTransforms() {
    for (NodeHandle root : dirtySet) {
        propagateWorldTransform(root);
    }
    dirtySet.clear();
    // ...clear all node.dirty flags...
}

void propagateWorldTransform(NodeHandle node) {
    // Always update world transform for all nodes in the subtree
    node.worldTransform = (node.parent != INVALID_HANDLE)
        ? nodes[node.parent].worldTransform * node.localTransform
        : node.localTransform;
    node.dirty = false;
    changedNodes.insert(node); // Use a set to avoid duplicates
    for (NodeHandle child : node.children) {
        propagateWorldTransform(child);
    }
}

const std::unordered_set<NodeHandle>& getChangedNodes() const {
    return changedNodes;
}

void clearChangedNodes() {
    changedNodes.clear();
}

bool isNodeDirty(NodeHandle node) const {
    nodes[node].dirty;
}
```

---

## 9. Observer Pattern Integration

- The Scene module can act as a subject, notifying observers (e.g., CullingSystem, DrawDataManager) when node transforms change. This enables decoupled, event-driven synchronization of per-system data. See [Observer Pattern](../core/observer_pattern.md) for details.
- **Testability:** Observer registration is explicit and can be performed with either real or mock systems, supporting robust unit and integration testing.

---

## 10. Example Usage

```cpp
// Create a node and attach mesh data
NodeHandle node = scene.addNode();
meshSystem.addMesh({ node, meshAsset });

// Update transforms and synchronize with systems
scene.setLocalTransform(node, newLocalTransform);
scene.finalizeForRendering(); // Performs transform propagation and notifies observers of changed nodes, then clears the changed set

// Propagate transforms only down to a specific node (and its ancestors if needed):
scene.propagateTransformsTo(targetNode); // Only updates the chain needed for targetNode to be up to date

// Query a node's world transform, always up to date (will propagate down to node if dirty):
glm::mat4 world = scene.getWorldTransform(node); // Default: updateIfDirty = true

// Query a node's world transform, without forcing propagation (may be stale):
glm::mat4 cachedWorld = scene.getWorldTransform(node, false);

// Set a node's world transform, computing the correct local transform automatically:
glm::mat4 desiredWorld = ...;
scene.setWorldTransform(node, desiredWorld); // Will update world and local transform to match. Will always propagate to parent if dirty

// Traverse the scene graph
scene.traverse([&](NodeHandle n) {
    glm::mat4 world = scene.getWorldTransform(n); // Always up to date
    // ...
});

// Find all mesh data for rendering
for (const auto& mesh : meshSystem.meshes) {
    glm::mat4 world = scene.getWorldTransform(mesh.node);
    // ...
}
```

---

## 11. Suggested Tests for SceneManager

### 11.1 Scene Lifecycle Tests
- **Add/Remove Scene:** Add multiple scenes, verify they are retrievable by name. Remove scenes and ensure they are no longer accessible.
- **Active Scene Switching:** Set and get the active scene, verify correct switching and retrieval.

### 11.2 Node Management Tests
- **Add/Remove Nodes:** Add nodes (with and without parents), verify hierarchy and parent/child relationships. Remove nodes and ensure all descendants are also removed.

### 11.3 Transform Propagation Tests
- **Transform Propagation:**
  - Set up a scene with a hierarchy (e.g., root → child → grandchild).
  - Set a local transform on a node (e.g., the root or an intermediate node).
  - Call `finalizeForRendering()` on the scene (which will perform transform propagation internally).
  - Verify that all descendants have correct world transforms (compare to expected matrices).
  - Verify that the set of changed nodes includes the node and all affected descendants.

#### Example (Pseudocode)
```cpp
Scene scene;
auto root = scene.addNode();
auto child = scene.addNode(root);
auto grandchild = scene.addNode(child);

scene.setLocalTransform(root, initialRootTransform);
scene.setLocalTransform(child, initialChildTransform);
scene.setLocalTransform(grandchild, initialGrandchildTransform);

scene.setLocalTransform(root, newRootTransform);
scene.finalizeForRendering();

assert(scene.getLocalTransform(child) == expectedChildWorldTransform);
assert(scene.getLocalTransform(grandchild) == expectedGrandchildWorldTransform);
assert(changedNodes.contains(child));
assert(changedNodes.contains(grandchild));
```

### 11.4 System Synchronization Tests
- **Transform Propagation and System Sync:**
  - Set up a scene with a hierarchy (e.g., root → child → grandchild).
  - Attach mock or test versions of systems (e.g., MockCullingSystem, MockDrawDataManager) that record when their observer callbacks (e.g., `onNodeTransformsChanged`) are called and what data they receive.
  - Set a local transform on a node in the scene.
  - Call `finalizeForRendering()` on the scene (which will perform transform propagation and notify observers).
  - Verify that:
    - All descendants have correct world transforms (compare to expected matrices).
    - The set of changed nodes includes the node and all affected descendants.
    - The mock systems received the correct set of changed nodes and updated their internal transforms accordingly.
    - If systems update asynchronously, simulate or wait for the update to complete before checking results.
    - For more robust tests, check that unrelated nodes/systems are not updated.

#### Example (Pseudocode)
```cpp
Scene scene;
auto root = scene.addNode();
auto child = scene.addNode(root);
auto grandchild = scene.addNode(child);

MockCullingSystem cullingSystem;
MockDrawDataManager drawDataManager;

scene.setLocalTransform(root, initialRootTransform);
scene.setLocalTransform(child, initialChildTransform);
scene.setLocalTransform(grandchild, initialGrandchildTransform);

scene.setLocalTransform(root, newRootTransform);
scene.finalizeForRendering(); // Performs transform propagation and notifies observers

// Assertions
assert(scene.getLocalTransform(child) == expectedChildWorldTransform);
assert(scene.getLocalTransform(grandchild) == expectedGrandchildWorldTransform);
assert(cullingSystem.lastSyncedNodes == /* expected changed nodes */);
assert(drawDataManager.lastSyncedNodes == /* expected changed nodes */);
assert(cullingSystem.transformsInSyncWithScene(scene));
assert(drawDataManager.transformsInSyncWithScene(scene));
```

### 11.5 Serialization/Deserialization Tests
- **Scene Serialization/Deserialization:**
  - Add a scene, set some transforms, and associate some data.
  - Serialize the scene to a file.
  - Deserialize the scene from the file into a new scene manager instance.
  - Verify that the new scene is structurally identical to the original (same hierarchy, transforms, data associations).
  - Test with various scene complexities (number of nodes, depth of hierarchy, amount of data).

---

## Coordination with Core System

The workflow for transform propagation, system synchronization, and integration with the main renderer loop is now fully specified in [Core System Coordination](../core/core_system_coordination.md). Please refer to that document for the latest and most accurate process.

- The scene module handles transform propagation, dirty tracking, and changed node collection.
- After all transform updates, `finalizeForRendering()` must be called to propagate transforms and notify observers.
- All dependent systems (rendering, culling, physics, etc.) must synchronize their data in response to these notifications, before rendering begins.
- This ensures all systems operate on up-to-date transforms and remain in sync with the scene state.

This section is intentionally concise; always consult the core coordination document for authoritative details.
