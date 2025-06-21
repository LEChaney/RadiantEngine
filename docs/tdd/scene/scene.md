# Scene Module Design Document

## 1. Purpose

The scene module is responsible for representing and managing the logical structure and transform hierarchy of the virtual world on the CPU side. It provides a minimal, flat scene graph for hierarchical transforms, and serves as the central point of association for all per-system data (rendering, lighting, physics, etc.).

---

## 2. Design Principles

- **Minimalism:** The scene graph only manages hierarchy and transforms. No node types, no polymorphism, no component system.
- **Flat Storage:** All nodes are stored in a flat array or pool, referenced by a `NodeHandle` (integer or struct).
- **Separation of Data:** All system-specific data (meshes, lights, physics, etc.) is stored externally, associated to nodes via `NodeHandle`.
- **Explicit Association:** Systems maintain their own data arrays, each entry referencing a node. The scene graph is agnostic to what data is attached to each node.
- **Cache-Friendly:** Flat storage and separation of data enable efficient traversal and system updates.

---

## 3. Scene Graph Structure

### Node (internal structure)
- `NodeHandle parent`
- `std::vector<NodeHandle> children`
- `glm::mat4 localTransform`
- `glm::mat4 worldTransform`
- `std::string name`
- `bool dirty` // **True if this node (or its ancestors) need world transform update**

All nodes are stored in a flat array or pool, indexed by `NodeHandle`.

---

## 4. Public API

### Scene
- `NodeHandle addNode(NodeHandle parent = INVALID_HANDLE)` – Create a new node, optionally as a child of `parent`.
- `void removeNode(NodeHandle)` – Remove a node and its descendants.
- `std::vector<NodeHandle> getRootNodes()` – Get handles of root nodes.
- `void traverse(std::function<void(NodeHandle)>, NodeHandle start = INVALID_HANDLE)` – Traverse the hierarchy, visiting each node by handle, starting at `start` (default: all roots).
- `NodeHandle findNodeByName(const std::string&)` – Find a node by name.
- `glm::mat4 getNodeTransform(NodeHandle)` – Get local/world transform for a node.
- `void setNodeTransform(NodeHandle, const glm::mat4&)` – Set local transform for a node. **Marks the node and all descendants as dirty for transform propagation.**
- `NodeHandle getParent(NodeHandle)` / `std::vector<NodeHandle> getChildren(NodeHandle)` – Query parent/children.
- `void loadFromFile(path)` / `void saveToFile(path)` – Load/save scene graph and associations.
  - *Note: These functions will select an appropriate loader or saver implementation under the hood (e.g., the GLTF scene loader for `.gltf`/`.glb` files). Initially, only the GLTF loader will be implemented. Saving is not planned for the initial release.*
- `auto getTransformBuffer()` – Returns a buffer or list of transforms for GPU upload.
- `void updateWorldTransforms()` – Propagate local-to-world transforms for all dirty nodes and their descendants. **Clears the dirty set and updates the changed nodes set.**
- `const std::unordered_set<NodeHandle>& getChangedNodes() const` – Returns a const reference to the set of nodes whose world transforms changed during all propagations since the last explicit clear. **This set is only cleared by the user.**
- `void clearChangedNodes()` – Clears the set of changed nodes.
- `bool isNodeDirty(NodeHandle node) const` – Returns true if the node is dirty. This is useful for checking whether a node's world transform is up to date.

---

## 5. System Data (External to Scene Graph)

Each system (rendering, lighting, physics, etc.) maintains its own data arrays, each entry referencing a `NodeHandle`.

**Example:**
- `struct MeshData { NodeHandle node; Mesh* mesh; ... }`
- `struct LightData { NodeHandle node; LightParams params; ... }`

Systems may maintain registries/maps for quick lookup from `NodeHandle` to data.

---

## 6. Association and Traversal

- The scene graph is responsible for transform propagation and hierarchy only.
- Systems are responsible for associating their data to nodes and updating as needed.
- To find all mesh data, iterate the mesh data array; to find all lights, iterate the light data array, etc.
- To find the node for a given data entry, use the stored `NodeHandle`.
- **Transform Synchronization:** After calling `updateWorldTransforms()`, the scene provides the set of changed nodes to systems (CullingSystem, DrawDataManager, etc.) for synchronization via their respective `syncTransforms` APIs.

---

## 7. SceneManager

- `void addScene(std::shared_ptr<Scene>)` – Adds a new scene to the manager.
- `void removeScene(const std::string& name)` – Removes a scene by name.
- `std::shared_ptr<Scene> getScene(const std::string& name)` – Retrieves a scene by name.
- `std::shared_ptr<Scene> getActiveScene()` – Returns the currently active scene.
- `void setActiveScene(const std::string& name)` – Sets the active scene by name.
- `const std::vector<std::shared_ptr<Scene>>& getAllScenes()` – Returns all loaded scenes.

---

## 8. Transform Propagation, Dirtying, and Synchronization (Logic & Pseudocode)

### 8.1 Dirty Flag Propagation and Minimal Dirty Set

- When a node's local transform is changed via `setNodeTransform`, that node **and all of its descendants** have their `dirty` flag immediately set to true. This ensures that checking whether a node's world transform is up to date is a simple matter of checking its own `dirty` flag.
- To ensure efficient propagation, the minimal dirty set algorithm is used to maintain only the minimal set of dirty roots. When marking a node dirty, if its parent is already dirty, no further action is needed (the parent's propagation will cover this node). Otherwise, the node is added to the dirty set, and all descendants are removed from the dirty set (since they will be covered by this node's propagation).

### 8.2 World Transform Propagation

- The method `updateWorldTransforms()` traverses the scene graph, propagating world transforms from parents to children for all dirty roots and their descendants. It updates the world transform for each affected node and clears their `dirty` flag.
- After `updateWorldTransforms()` completes, all nodes are removed from the dirty set and their `dirty` flags are cleared. The dirty state is now clear, and the scene is considered up to date until the next call to `setNodeTransform`.

### 8.3 Changed Nodes Tracking

- As each node's world transform is updated, its handle is added to a `changedNodes` set. This set records all nodes whose world transforms were modified during the last propagation(s). The set is only cleared when the user explicitly calls `clearChangedNodes()`.
- The set persists and accumulates across multiple calls to `updateWorldTransforms()` until the user explicitly clears it. This allows for multiple transform propagations within a frame if needed (e.g., for dependency updates), without losing track of which nodes have changed.

### 8.4 Rationale

By immediately setting the `dirty` flag on a node and all its descendants when `setNodeTransform` is called, checking whether a node's world transform is up to date is a simple, fast operation: just check the node's own `dirty` flag. The minimal dirty set ensures that each affected transform chain is only updated once, regardless of the order or number of `setNodeTransform` calls, and avoids redundant updates even in complex scenarios.

### 8.5 Implementation Pseudocode

```cpp
void setNodeTransform(NodeHandle node, const glm::mat4& local) {
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

void updateWorldTransforms() {
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

## 9. Example Usage

```cpp
// Create a node and attach mesh data
NodeHandle node = scene.addNode();
meshSystem.addMesh({ node, meshAsset });

// Update transforms and synchronize with systems
scene.setNodeTransform(node, newLocalTransform);
scene.updateWorldTransforms(); // Propogates updates from minimal dirty root set
const auto& changedNodes = scene.getChangedNodes();
cullingSystem.syncTransforms(changedNodes, scene);
drawDataManager.syncTransforms(changedNodes, scene);
scene.clearChangedNodes(); // User must explicitly clear the set after all systems have synchronized

// Traverse the scene graph
scene.traverse([&](NodeHandle n) {
    glm::mat4 world = scene.getNodeTransform(n);
    // ...
});

// Find all mesh data for rendering
for (const auto& mesh : meshSystem.meshes) {
    glm::mat4 world = scene.getNodeTransform(mesh.node);
    // ...
}
```

---

## 10. Notes

- The scene graph is intentionally minimal and agnostic to system data.
- All per-system data is managed externally and associated via `NodeHandle`.
- Transform synchronization with systems is explicit and must be performed after transform updates.
- This design enables cache-friendly, parallel, and system-oriented updates.

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
  - Call `updateWorldTransforms()` on the scene.
  - Verify that all descendants have correct world transforms (compare to expected matrices).
  - Verify that the set of changed nodes includes the node and all affected descendants.

#### Example (Pseudocode)
```cpp
Scene scene;
auto root = scene.addNode();
auto child = scene.addNode(root);
auto grandchild = scene.addNode(child);

scene.setNodeTransform(root, initialRootTransform);
scene.setNodeTransform(child, initialChildTransform);
scene.setNodeTransform(grandchild, initialGrandchildTransform);

scene.setNodeTransform(root, newRootTransform);
scene.updateWorldTransforms();

assert(scene.getNodeTransform(child) == expectedChildWorldTransform);
assert(scene.getNodeTransform(grandchild) == expectedGrandchildWorldTransform);
assert(changedNodes.contains(child));
assert(changedNodes.contains(grandchild));
```

### 11.4 System Synchronization Tests
- **Transform Propagation and System Sync:**
  - Set up a scene with a hierarchy (e.g., root → child → grandchild).
  - Attach mock or test versions of systems (e.g., MockCullingSystem, MockDrawDataManager) that record when their `syncTransforms` methods are called and what data they receive.
  - Set a local transform on a node in the scene.
  - Call `updateWorldTransforms()` on the scene.
  - Call each system's `syncTransforms(scene.getChangedNodes(), scene)`.
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

scene.setNodeTransform(root, initialRootTransform);
scene.setNodeTransform(child, initialChildTransform);
scene.setNodeTransform(grandchild, initialGrandchildTransform);

scene.setNodeTransform(root, newRootTransform);
scene.updateWorldTransforms();
auto changedNodes = scene.getChangedNodes();

// System synchronization
cullingSystem.syncTransforms(changedNodes, scene);
drawDataManager.syncTransforms(changedNodes, scene);

// Assertions
assert(scene.getNodeTransform(child) == expectedChildWorldTransform);
assert(scene.getNodeTransform(grandchild) == expectedGrandchildWorldTransform);
assert(cullingSystem.lastSyncedNodes == changedNodes);
assert(drawDataManager.lastSyncedNodes == changedNodes);
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

The coordination of transform propagation (`updateWorldTransforms`), system synchronization (`syncTransforms`), and the main renderer loop is now documented in detail in [Core System Coordination](../core/core_system_coordination.md). Refer to that document for the authoritative workflow and lifecycle. This section is intentionally brief to avoid duplication.

- The scene module is responsible for efficient transform propagation and tracking changed nodes.
- Systems must synchronize their data after transforms are updated and before rendering, as described in the core coordination document.
