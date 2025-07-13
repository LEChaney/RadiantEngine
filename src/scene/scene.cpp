#include "scene.h"
#include <cassert>

// SceneNode and SceneNodeKey are defined in scene.h

// Scene implementation
Scene::Scene() {
    // Create root node
    root_key = nodes.add(SceneNode{
        SceneNodeKey::null(), // parent_key
        {},                   // children_keys
        glm::mat4(1.0f),      // local_transform
        glm::mat4(1.0f),      // world_tansform
        false,                // dirty
        "root"                // name
    });
}

Scene::~Scene() = default;

SceneNodeKey Scene::add_node(SceneNodeKey parent_key, const std::string& name) {
    if (parent_key.is_null()) {
        parent_key = root_key;
    }

    SceneNodeKey new_node_key = nodes.add(SceneNode{
        parent_key,      // parent_key
        {},              // children_keys
        glm::mat4(1.0f), // local_transform
        glm::mat4(1.0f), // world_tansform
        true,            // dirty
        name             // name
    });

    SceneNode& parent_node = nodes[parent_key];
    parent_node.children_keys.push_back(new_node_key);

    if (!parent_node.dirty) {
        // Top of a dirty tree, so insert into minimal dirty set
        minimal_dirty_set.insert(new_node_key);
    }

    return new_node_key;
}

SceneNodeKey Scene::get_root_key() const {
    return root_key;
}

SceneNodeKey Scene::get_parent_key(SceneNodeKey node_key) const {
    return nodes.get<SceneNode>(node_key)->parent_key;
}

const std::vector<SceneNodeKey>& Scene::get_children_keys(SceneNodeKey node_key) const {
    return nodes.get<SceneNode>(node_key)->children_keys;
}

const SceneNode *Scene::get_node(SceneNodeKey node_key) const
{
    return nodes.get<SceneNode>(node_key);
}

SceneNode *Scene::get_node(SceneNodeKey node_key)
{
    return const_cast<SceneNode*>(static_cast<const Scene*>(this)->get_node(node_key));
}

glm::mat4 Scene::get_local_transform(SceneNodeKey node_key) const
{
    return nodes[node_key].local_transform;
}

glm::mat4 Scene::get_world_transform(SceneNodeKey node_key, bool update_if_dirty)
{
    SceneNode& node = nodes[node_key];
    if (update_if_dirty && node.dirty) {
        propagate_transforms_to(node_key);
    }
    return node.world_tansform;
}

glm::mat4 Scene::get_world_transform(SceneNodeKey node_key, bool update_if_dirty) const
{
    const SceneNode& node = nodes[node_key];
    if (update_if_dirty && node.dirty) {
        assert(false && 
               "This should not be called with update_if_dirty=true on const Scene, "
               "use non-const version to update transforms");
    }
    return node.world_tansform;
}

void Scene::set_local_transform(SceneNodeKey node_key, const glm::mat4& local) {
    SceneNode& node = nodes[node_key];
    if (node.local_transform == local) {
        return; // No change, no need to dirty
    }
    node.local_transform = local;
    mark_dirty(node_key);
}

void Scene::mark_dirty(SceneNodeKey node_key) {
    SceneNode& node = nodes[node_key];
    if (node.dirty) {
        return;
    }

    mark_dirty_recursive(node_key);
    
    // If we were not already dirty, then this is the top of a dirty tree,
    // so we need to insert it into the minimal dirty set
    minimal_dirty_set.insert(node_key);
}

void Scene::mark_dirty_recursive(SceneNodeKey node_key, bool remove_from_minimal_dirty_set) {
    SceneNode& node = nodes[node_key];

    if (node.dirty) {
        // We only need to remove nodes from the minimal dirty set if they 
        // are already dirty. This dirty node will also already be covering
        // all its descendants, so we can skip them.
        if (remove_from_minimal_dirty_set) {
            minimal_dirty_set.erase(node_key);
        }

        return;
    }

    node.dirty = true;
    for (const auto& child_key : node.children_keys) {
        mark_dirty_recursive(child_key, remove_from_minimal_dirty_set);
    }
}

bool Scene::is_covered_by_dirty_set(SceneNodeKey node_key) const
{
    SceneNodeKey parent_key = nodes[node_key].parent_key;
    if (!parent_key.is_null() && nodes[parent_key].dirty) {
        return true; // Parent is dirty, so this node is covered
    }
    return false; // No dirty parent, this node is not covered
}

void Scene::propagate_subtree(SceneNodeKey node_key, const glm::mat4& parent_world, SceneNodeKeySet& changed_nodes) {
    SceneNode& node = nodes[node_key];
    glm::mat4 prev_world = node.world_tansform;
    node.world_tansform = parent_world * node.local_transform;
    node.dirty = false;
    if (node.world_tansform != prev_world) {
        changed_nodes.insert(node_key);
    }
    for (const auto& child_key : node.children_keys) {
        propagate_subtree(child_key, node.world_tansform, changed_nodes);
    }
}

void Scene::propagate_transforms_to(SceneNodeKey target_key) {
    if (target_key.is_null() || !nodes[target_key].dirty) {
        return; // Nothing to propagate
    }

    // Propogate to parent first
    propagate_transforms_to(nodes[target_key].parent_key);
    
    // The parents world transform is now up to date,
    // so we can use it to compute the targets world transform
    SceneNode& target_node = nodes[target_key];
    SceneNodeKey parent_key = target_node.parent_key;
    glm::mat4 prev_world = target_node.world_tansform;
    glm::mat4 parent_world = parent_key.is_null() ? 
        glm::mat4(1.0f) : 
        nodes[parent_key].world_tansform;
    target_node.world_tansform = parent_world * target_node.local_transform;

    // If the world transform has changed, add to changed nodes
    if (target_node.world_tansform != prev_world) {
        changed_nodes.insert(target_key);
    }

    // Set dirty state to false for the target node
    target_node.dirty = false;
    
    // Propogate down minimal dirty set state to children, 
    // since they're still dirty and need to be covered
    if (minimal_dirty_set.erase(target_key)) {
        for (const auto& child_key : target_node.children_keys) {
            minimal_dirty_set.insert(child_key);
        }
    }
}

void Scene::propagate_transforms() {
    // Propagate transforms for all minimal dirty roots
    for (SceneNodeKey dirty_key : minimal_dirty_set) {
        glm::mat4 parent_world = glm::mat4(1.0f);
        SceneNodeKey parent_key = nodes[dirty_key].parent_key;
        if (!parent_key.is_null()) {
            parent_world = nodes[parent_key].world_tansform;
        }
        propagate_subtree(dirty_key, parent_world, changed_nodes);
    }
    minimal_dirty_set.clear();
}

void Scene::finalize_and_notify() {
    propagate_transforms();
    // Notify observers of changed nodes
    for (auto* observer : observers) {
        assert(observer && "Observer should not be null");
        observer->on_scene_nodes_changed(changed_nodes);
    }
    changed_nodes.clear();
}

// Observer registration implementations
void Scene::add_observer(ISceneObserver* observer) {
    if (observer && std::find(observers.begin(), observers.end(), observer) == observers.end()) {
        observers.push_back(observer);
    }
}

void Scene::remove_observer(ISceneObserver* observer) {
    auto it = std::remove(observers.begin(), observers.end(), observer);
    if (it != observers.end()) {
        observers.erase(it, observers.end());
    }
}

const SceneNodeKeySet& Scene::get_changed_nodes() const {
    return changed_nodes;
}

bool Scene::is_node_dirty(SceneNodeKey node_key) const
{
    return nodes[node_key].dirty;
}

const SceneNodeKeySet &Scene::get_minimal_dirty_set() const
{
    return minimal_dirty_set;
}
