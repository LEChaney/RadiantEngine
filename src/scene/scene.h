#pragma once

#include "utils/containers/slotmap.h"

#include "ankerl/unordered_dense.h"
#include "glm/mat4x4.hpp"

#include <vector>
#include <string>
#include <functional>
#include <cstdint>

// Forward declarations
struct SceneNode;
class Scene;
class SceneManager;

using SceneNodeKey = SlotMap<SceneNode>::Key;
using SceneKey = SlotMap<Scene>::Key;
using SceneNodeKeySet = ankerl::unordered_dense::set<SceneNodeKey>;

struct SceneNode {
    SceneNodeKey parent_key;
    std::vector<SceneNodeKey> children_keys;
    glm::mat4 local_transform{1.0f};
    glm::mat4 world_tansform{1.0f};
    bool dirty = true;
    std::string name;
};

class Scene {
public:
    Scene();
    ~Scene();

    SceneNodeKey add_node(
        SceneNodeKey parent_key = SceneNodeKey::null(),
        const std::string& name = ""
    );
    void remove_node(SceneNodeKey node_key);

    SceneNodeKey get_root_key() const;
    SceneNodeKey get_parent_key(SceneNodeKey node_key) const;
    const std::vector<SceneNodeKey>& get_children_keys(SceneNodeKey node_key) const;
    SceneNodeKey find_node_by_name(const std::string& name) const;
    const SceneNode* get_node(SceneNodeKey node_key) const;
    SceneNode* get_node(SceneNodeKey node_key);

    /**
     * Returns the local transform of the specified node.
     * Does not update or propagate transforms; returns the stored value.
     */
    glm::mat4 get_local_transform(SceneNodeKey node_key) const;

    /**
     * Returns the world transform of the specified node.
     * If update_if_dirty is true (default), propagates transforms for the node 
     * and its ancestors if dirty, ensuring the returned transform is up to date.
     * If false, returns the cached world transform (may be stale).
     */
    glm::mat4 get_world_transform(SceneNodeKey node_key, bool update_if_dirty = true);

    /**
     * Const version: Returns the cached world transform of the specified node.
     * If update_if_dirty is true and the node is dirty, triggers an assert (cannot
     * update in const context).
     */
    glm::mat4 get_world_transform(SceneNodeKey node_key, bool update_if_dirty = true) const;

    /**
     * Sets the local transform of the specified node.
     * Marks the node and all descendants as dirty for transform propagation.
     * Updates the minimal dirty set accordingly.
     */
    void set_local_transform(SceneNodeKey node_key, const glm::mat4& local);

    /**
     * Sets both the world transform and the local transform of the node.
     * Computes and sets the local transform so that parent_world * local = world.
     * Propagates transforms up to the parent if dirty before computing.
     * Clears the dirty flag on the node itself (since its world transform is set).
     * Marks all descendants as dirty for propagation.
     */
    void set_world_transform(SceneNodeKey node_key, const glm::mat4& world);

    /**
     * Propagates transforms from the nearest dirty ancestor (or itself) down to the
     * specified target node, updating only the necessary chain. Stops as soon as the
     * target node is up to date and no longer dirty. Updates the minimal dirty set
     * accordingly.
     */
    void propagate_transforms_to(SceneNodeKey target_key);

    /**
     * Propagates local-to-world transforms for all dirty nodes and their descendants.
     * Clears the minimal dirty set and updates the changed nodes set.
     */
    void propagate_transforms();

    /**
     * Performs final transform propagation (calls propagate_transforms internally if
     * needed), notifies all registered observers of node transform changes via the
     * observer pattern, passing the set of changed nodes, and then clears the changed
     * nodes set. Should be called once per frame, after all transform updates and
     * before rendering.
     */
    void finalize_and_notify();

    const SceneNodeKeySet& get_changed_nodes() const;
    bool is_node_dirty(SceneNodeKey node_key) const;

    // For testing and debugging: expose minimal_dirty_set
    const SceneNodeKeySet& get_minimal_dirty_set() const;

private:
    SlotMap<SceneNode> nodes;
    SceneNodeKey root_key;

    // Set of top-most dirty nodes that need transform propagation.
    SceneNodeKeySet minimal_dirty_set;

    // Set of nodes that have changed since the last frame. Used for observer
    // notifications.
    SceneNodeKeySet changed_nodes;

    // Marks the node and all its descendants as dirty. Also updates the minimal
    // dirty set such that only the top-most dirty ancestor is kept.
    void mark_dirty(SceneNodeKey node_key);

    // Recursively marks nodes as dirty and optionally removes them from
    // minimal_dirty_set.
    void mark_dirty_recursive(
        SceneNodeKey node_key,
        bool dirty_this_node = true,
        bool remove_from_minimal_dirty_set = true);
    bool is_covered_by_dirty_set(SceneNodeKey node_key) const;

    // Helper: propagate transforms down a subtree.
    void propagate_subtree(
        SceneNodeKey node_key,
        const glm::mat4& parent_world,
        SceneNodeKeySet& changed_nodes);

    // Should be called after all observers have been notified of changes.
    void clear_changed_nodes();
};

class ISceneManagerObserver {
public:
    virtual ~ISceneManagerObserver() = default;
    virtual void on_active_scenes_changed(
        const std::vector<SceneKey>& new_active_scenes
    ) = 0;
};

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    SceneKey add_scene();
    void remove_scene(SceneKey scene);
    Scene* get_scene(SceneKey scene);
    const Scene* get_scene(SceneKey scene) const;
    Scene* get_active_scene() const;
    std::vector<SceneKey> get_active_scenes() const;
    void set_active_scene(SceneKey scene);
    void set_active_scenes(const std::vector<SceneKey>& scenes);
    void add_observer(ISceneManagerObserver* observer);
    void remove_observer(ISceneManagerObserver* observer);

private:
    SlotMap<Scene> scenes;
    std::vector<SceneKey> active_scenes;
    std::vector<ISceneManagerObserver*> observers;
    // ...other private members as needed...
};
