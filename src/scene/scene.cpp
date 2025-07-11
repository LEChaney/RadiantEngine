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
    if (!nodes.is_valid(parent_key)) {
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
    SceneNode* parent_node = nodes.get<SceneNode>(parent_key);
    parent_node->children_keys.push_back(new_node_key);
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

// ...other methods not yet implemented...
