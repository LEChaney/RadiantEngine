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
        true,                 // dirty
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
        false,           // dirty
        name             // name
    });
    SceneNode* parent_node = nodes.get<SceneNode>(parent_key);
    parent_node->children_keys.push_back(new_node_key);
    mark_dirty(new_node_key); // Mark new node dirty and propagate dirty state
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

void Scene::set_local_transform(SceneNodeKey node_key, const glm::mat4& local) {
    SceneNode& node = nodes[node_key];
    node.local_transform = local;
    mark_dirty(node_key);
}

void Scene::mark_dirty(SceneNodeKey node_key) {
    SceneNode& node = nodes[node_key];
    if (node.dirty) {
        return;
    }

    bool covered = is_covered_by_dirty_set(node_key);
    // Only remove descendants if not covered
    mark_dirty_recursive(node_key, true, !covered);

    if (!covered) {
        minimal_dirty_set.insert(node_key);
    }
}

void Scene::mark_dirty_recursive(SceneNodeKey node_key, bool dirty_this_node, bool remove_from_minimal_dirty_set) {
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

    node.dirty = dirty_this_node;
    for (const auto& child_key : node.children_keys) {
        mark_dirty_recursive(child_key, true, remove_from_minimal_dirty_set);
    }
}

bool Scene::is_covered_by_dirty_set(SceneNodeKey node_key) const
{
    SceneNodeKey cur_key = node_key;
    while (cur_key != SceneNodeKey::null() && nodes[cur_key].dirty) {
        if (minimal_dirty_set.find(cur_key) != minimal_dirty_set.end()) {
            return true;
        }
        
        cur_key = nodes[cur_key].parent_key;
    }
    return false;
}
