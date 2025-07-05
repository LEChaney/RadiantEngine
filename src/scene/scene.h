#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <functional>
#include <glm/mat4x4.hpp>
#include <cstdint>

#include "utils/containers/slotmap.h"


// Forward declarations
/// @brief Internal node structure for the scene graph.
struct SceneNode;
/// @brief Main scene class, manages node hierarchy and transforms.
class Scene;
/// @brief Manages all loaded scenes and active scene selection.
class SceneManager;


/// @brief Handle type for referencing nodes in the scene graph.
using NodeHandle = SlotMap<SceneNode>::Handle;
/// @brief Handle type for referencing scenes in the scene manager.
using SceneHandle = SlotMap<Scene>::Handle;


/// @brief Internal node structure for the scene graph hierarchy.
///        Not intended for direct use outside the scene module.
struct SceneNode {
    /// Parent node handle (or null for root).
    NodeHandle parent;
    /// Child node handles.
    std::vector<NodeHandle> children;
    /// Local transform (relative to parent).
    glm::mat4 local_transform{1.0f};
    /// World transform (absolute, cached, updated via propagation).
    glm::mat4 world_tansform{1.0f};
    /// Node name (optional, for lookup/debugging).
    std::string name;
    /// Dirty flag: true if this node or any ancestor needs world transform update.
    bool dirty = true;
};

// Scene class
/// @brief Scene graph class. Manages node hierarchy, transforms, and change tracking.
///        All per-system data (meshes, lights, etc.) is associated externally via 
///        NodeHandle.
class Scene {
public:
    /// Construct a new scene (creates a root node automatically).
    Scene();
    ~Scene();

    /// @brief Get the handle to the root node of the scene.
    NodeHandle get_root_node() const;

    /// @brief Add a new node as a child of the given parent 
    ///        (or root if parent is null).
    /// @param parent The parent node handle (or NodeHandle::null for root).
    /// @param name   The name of the new node (optional).
    /// @return Handle to the newly created node.
    NodeHandle add_node(
        NodeHandle parent = NodeHandle::null(), 
        const std::string& name = ""
    );

    /// @brief Remove a node and all its descendants from the scene.
    /// @param node The node to remove.
    void remove_node(NodeHandle node);

    /// @brief Get the parent of a node.
    /// @param node The node whose parent to query.
    /// @return The parent node handle.
    NodeHandle get_parent(NodeHandle node) const;

    /// @brief Get the children of a node.
    /// @param node The node whose children to query.
    /// @return Vector of child node handles.
    std::vector<NodeHandle> get_children(NodeHandle node) const;

    /// @brief Traverse the hierarchy, visiting each node by handle.
    /// @param visitor Function to call for each node handle.
    /// @param start   Node to start traversal from (default: root).
    void traverse(
        const std::function<void(NodeHandle)>& visitor,
        NodeHandle start = NodeHandle::null()
    ) const;

    /// @brief Find a node by name (returns null handle if not found).
    /// @param name The name to search for.
    /// @return Handle to the found node, or NodeHandle::null if not found.
    NodeHandle find_node_by_name(const std::string& name) const;

    /// @brief Get the local transform for a node.
    /// @param node The node to query.
    /// @return The local transform matrix.
    glm::mat4 get_local_transform(NodeHandle node) const;

    /// @brief Get the world transform for a node. If update_if_dirty is true, 
    ///        propagates transforms as needed.
    /// @param node            The node to query.
    /// @param update_if_dirty If true, propagates transforms if needed (default: true).
    /// @return The world transform matrix.
    glm::mat4 get_world_transform(NodeHandle node, bool update_if_dirty = true);

    /// @brief Set the local transform for a node (marks node and descendants dirty).
    /// @param node  The node to update.
    /// @param local The new local transform.
    void set_local_transform(NodeHandle node, const glm::mat4& local);

    /// @brief Set the world transform for a node (computes local transform, marks 
    ///        dirty).
    /// @param node  The node to update.
    /// @param world The new world transform.
    void set_world_transform(NodeHandle node, const glm::mat4& world);

    /// @brief Propagate transforms from the nearest dirty ancestor down to the target 
    ///        node.
    /// @param target The node to propagate transforms to.
    void propagate_transforms_to(NodeHandle target);

    /// @brief Propagate transforms for all dirty nodes and descendants.
    void propagate_transforms();

    /// @brief Finalize transforms and notify observers of changed nodes 
    ///        (call once per frame).
    void finalize_for_rendering();

    /// @brief Get the set of nodes whose world transforms changed since last clear.
    /// @return Reference to the set of changed node handles.
    const std::unordered_set<NodeHandle>& get_changed_nodes() const;

    /// @brief Clear the set of changed nodes.
    void clear_changed_nodes();

    /// @brief Check if a node is dirty (world transform not up to date).
    /// @param node The node to check.
    /// @return True if the node is dirty, false otherwise.
    bool is_node_dirty(NodeHandle node) const;

    /// @brief Load scene graph and associations from file.
    /// @param path Path to the file to load from.
    void load_from_file(const std::string& path);

    /// @brief Save scene graph and associations to file.
    /// @param path Path to the file to save to.
    void save_to_file(const std::string& path) const;

private:
    /// Slot map storage for all nodes in the scene.
    SlotMap<SceneNode> nodes;
    /// Handle to the always-present root node.
    NodeHandle scene_root;
    /// Minimal set of dirty roots for efficient transform propagation.
    std::unordered_set<NodeHandle> dirty_set;
    /// Set of nodes whose world transforms changed since last clear.
    std::unordered_set<NodeHandle> changed_nodes;
    // ...other private members as needed...
};

// SceneManager observer interface
/// @brief Observer interface for receiving notifications when the set of active
///        scenes changes.
class ISceneManagerObserver {
public:
    virtual ~ISceneManagerObserver() = default;
    /// Called when the set of active scenes changes.
    virtual void on_active_scenes_changed(
        const std::vector<SceneHandle>& new_active_scenes
    ) = 0;
};

// SceneManager class
/// @brief Manages all loaded scenes and the set of active scenes. Notifies 
///        observers on changes.
class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    /// @brief Add a new scene to the manager.
    /// @return Handle to the newly created scene.
    SceneHandle add_scene();

    /// @brief Remove a scene by handle.
    /// @param scene The scene handle to remove.
    void remove_scene(SceneHandle scene);

    /// @brief Get a scene by handle.
    /// @param scene The scene handle to retrieve.
    /// @return Pointer to the scene, or nullptr if not found.
    Scene* get_scene(SceneHandle scene);

    /// @brief Get a scene by handle (const).
    /// @param scene The scene handle to retrieve.
    /// @return Const pointer to the scene, or nullptr if not found.
    const Scene* get_scene(SceneHandle scene) const;

    /// @brief Get the currently active scene (if only one is supported).
    /// @return Pointer to the active scene, or nullptr if none.
    Scene* get_active_scene() const;

    /// @brief Get all currently active scenes (for multi-scene support).
    /// @return Vector of handles to all active scenes.
    std::vector<SceneHandle> get_active_scenes() const;

    /// @brief Set the active scene by handle.
    /// @param scene The scene handle to set as active.
    void set_active_scene(SceneHandle scene);

    /// @brief Set the set of active scenes (for multi-scene support).
    /// @param scenes Vector of scene handles to set as active.
    void set_active_scenes(const std::vector<SceneHandle>& scenes);

    /// @brief Register an observer to be notified when the active scene(s) change.
    /// @param observer Pointer to the observer to add.
    void add_observer(ISceneManagerObserver* observer);

    /// @brief Unregister an observer.
    /// @param observer Pointer to the observer to remove.
    void remove_observer(ISceneManagerObserver* observer);

private:
    /// Slot map storage for all scenes.
    SlotMap<Scene> scenes;
    /// List of currently active scenes.
    std::vector<SceneHandle> active_scenes;
    /// Registered observers for active scene changes.
    std::vector<ISceneManagerObserver*> observers;
    // ...other private members as needed...
};
