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

    glm::mat4 get_local_transform(SceneNodeKey node_key) const;
    glm::mat4 get_world_transform(SceneNodeKey node_key, bool update_if_dirty = true);
    void set_local_transform(SceneNodeKey node_key, const glm::mat4& local);
    void set_world_transform(SceneNodeKey node_key, const glm::mat4& world);

    void propagate_transforms_to(SceneNodeKey target);
    void propagate_transforms();
    void finalize_for_rendering();

    const SceneNodeKeySet& get_changed_nodes() const;
    bool is_node_dirty(SceneNodeKey node_key) const;

    // For testing and debugging: expose minimal_dirty_set
    const SceneNodeKeySet& get_minimal_dirty_set() const;

private:
    SlotMap<SceneNode> nodes;
    SceneNodeKey root_key;
    SceneNodeKeySet minimal_dirty_set;
    SceneNodeKeySet changed_nodes;

    void clear_changed_nodes();
    void mark_dirty(SceneNodeKey node_key);
    // Recursively marks nodes as dirty and optionally removes them from minimal_dirty_set
    void mark_dirty_recursive(
        SceneNodeKey node_key, 
        bool dirty_this_node = true, 
        bool remove_from_minimal_dirty_set = true);
    bool is_covered_by_dirty_set(SceneNodeKey node_key) const;

    // Helper: propagate transforms down a subtree
    void propagate_subtree(SceneNodeKey node_key, const glm::mat4& parent_world, SceneNodeKeySet& changed_nodes);
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
