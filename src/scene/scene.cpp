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

    SceneNodeKey new_node_key = nodes.add();
    SceneNode& new_node = nodes[new_node_key];
    new_node.parent_key = parent_key;
    new_node.name = name;
    new_node.dirty = true; // New nodes are always dirty initially

    SceneNode& parent_node = nodes[parent_key];
    parent_node.children_keys.push_back(new_node_key);

    if (!parent_node.dirty) {
        // Top of a dirty tree, so insert into minimal dirty set
        minimal_dirty_set.insert(new_node_key);
    }

    return new_node_key;
}

void Scene::remove_node(SceneNodeKey node_key, bool remove_all_descendants)
{
    assert(!node_key.is_null() && "Node key cannot be null");
    assert(node_key != root_key && "Cannot remove root node");

    SceneNode& node = nodes[node_key];

    SceneNodeKey parent_key = node.parent_key;
    SceneNode& parent_node = nodes[parent_key];

    if (remove_all_descendants) {
        // Remove all descendants recursively
        // Use a copy of children_keys to avoid modifying the vector while iterating
        std::vector<SceneNodeKey> children_keys_copy = node.children_keys;
        for (SceneNodeKey child_key : children_keys_copy) {
            remove_node(child_key, true);
        }
    }

    // Remove node from parent's children_keys
    auto& siblings = parent_node.children_keys;
    auto it = std::find(siblings.begin(), siblings.end(), node_key);
    if (it != siblings.end()) {
        std::swap(*it, siblings.back());
        siblings.pop_back();
    }

    // Patch children to parent
    for (SceneNodeKey child_key : node.children_keys) {
        SceneNode& child_node = nodes[child_key];
        child_node.parent_key = parent_key;
        parent_node.children_keys.push_back(child_key);
        // Mark child as dirty since its parent is changing
        mark_dirty(child_key);
    }

    // Update internal state tracking
    minimal_dirty_set.erase(node_key);
    changed_nodes.erase(node_key);
    removed_nodes.insert(node_key);

    // Remove node from slotmap
    nodes.remove(node_key);
}

void Scene::attach_node(SceneNodeKey node_key, SceneNodeKey new_parent_key) {
    assert(!node_key.is_null() && "Node key cannot be null");
    assert(!new_parent_key.is_null() && "New parent key cannot be null");
    assert(node_key != root_key && "Cannot attach root node to another parent");

    SceneNode& node = nodes[node_key];
    SceneNodeKey old_parent_key = node.parent_key;
    SceneNode& old_parent = nodes[old_parent_key];
    auto& siblings = old_parent.children_keys;
    // Remove node_key from siblings using swap-and-pop for efficiency
    auto it = std::find(siblings.begin(), siblings.end(), node_key);
    if (it != siblings.end()) {
        std::swap(*it, siblings.back());
        siblings.pop_back();
    }
    node.parent_key = new_parent_key;
    SceneNode& new_parent = nodes[new_parent_key];
    new_parent.children_keys.push_back(node_key);

    // Mark the node and its descendants as dirty
    mark_dirty(node_key);
}

void Scene::detach_node(SceneNodeKey node_key)
{
    attach_node(node_key, root_key);
}

SceneNodeKey Scene::get_root_key() const
{
    return root_key;
}

SceneNodeKey Scene::get_parent_key(SceneNodeKey node_key) const {
    return nodes.get<SceneNode>(node_key)->parent_key;
}

const std::vector<SceneNodeKey>& Scene::get_children_keys(SceneNodeKey node_key) const {
    return nodes.get<SceneNode>(node_key)->children_keys;
}

SceneNodeKey Scene::find_node_by_name(const std::string &name) const
{
    for (const auto& [key, node] : nodes) {
        if (node.name == name) {
            return key;
        }
    }
    return SceneNodeKey::null(); // Not found
}

const SceneNode* Scene::get_node(SceneNodeKey node_key) const
{
    return nodes.get<SceneNode>(node_key);
}

SceneNode* Scene::get_node(SceneNodeKey node_key)
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

    // Mark the node and all descendants as dirty
    // Skip updating dirty state if already dirty; covered by parent if so.
    // If node isn't dirty, parent also can't be dirty, so covered check 
    // isn't needed either.
    mark_dirty(node_key, true, true);
}

void Scene::set_world_transform(SceneNodeKey node_key, const glm::mat4 &world)
{
    SceneNode& node = nodes[node_key];
    if (node.world_tansform == world && !node.dirty) {
        // No change, no need to dirty
        return;
    }

    // Ensure the parent world transform is up to date
    SceneNodeKey parent_key = node.parent_key;
    propagate_transforms_to(parent_key);

    // Compute the local transform based on the parent world
    // P * L = W -> L = P^-1 * W
    glm::mat4 parent_world = parent_key.is_null() ? 
        glm::mat4(1.0f) : 
        nodes[parent_key].world_tansform;
    node.local_transform = glm::inverse(parent_world) * world;

    // Update world transform and mark as changed
    if (node.world_tansform != world) {
        node.world_tansform = world;
        changed_nodes.insert(node_key);
    }

    // Clear dirty state on this node and mark all descendants as dirty
    node.dirty = false;
    minimal_dirty_set.erase(node_key);
    for (const auto& child_key : node.children_keys) {
        // We CAN'T skip update on already dirty children, as they will need
        // to be added to the minimal dirty set still. We can skip the covered check
        // though, since the parent node is clean.
        mark_dirty(child_key, false, true);
    }
}

void Scene::mark_dirty(SceneNodeKey node_key, 
    bool skip_update_if_dirty, 
    bool skip_covered_check)
{
    SceneNode& node = nodes[node_key];
    if (skip_update_if_dirty && node.dirty) {
        return;
    }

    mark_dirty_recursive(node_key);

    // Add to minimal dirty set if not already covered by parent.
    if (skip_covered_check ||
        node.parent_key.is_null() ||
        !nodes[node.parent_key].dirty) 
    {
        minimal_dirty_set.insert(node_key);
    }
}

void Scene::mark_dirty_recursive(SceneNodeKey node_key) {
    SceneNode& node = nodes[node_key];
    if (node.dirty) {
        // Remove from minimal dirty set if already dirty; 
        // descendants are already covered by ancestor.
        minimal_dirty_set.erase(node_key);
        return;
    }

    node.dirty = true;
    for (const auto& child_key : node.children_keys) {
        mark_dirty_recursive(child_key);
    }
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

void Scene::propagate_transforms_to(SceneNodeKey target_key)
{
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
        if (!changed_nodes.empty()) {
            observer->on_scene_nodes_changed(changed_nodes);
        }
        if (!removed_nodes.empty()) {
            observer->on_scene_nodes_removed(removed_nodes);
        }
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

bool Scene::is_node_dirty(SceneNodeKey node_key) const
{
    return nodes[node_key].dirty;
}

const SceneNodeKeySet &Scene::get_minimal_dirty_set() const
{
    return minimal_dirty_set;
}

const SceneNodeKeySet& Scene::get_changed_nodes() const {
    return changed_nodes;
}

const SceneNodeKeySet& Scene::get_removed_nodes() const {
    return removed_nodes;
}