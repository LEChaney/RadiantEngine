#include "scene/Scene.h"
#include "core/CoreDefs.h"
#include <algorithm>

// SceneNode and SceneNodeKey are defined in Scene.h

// Scene implementation
Scene::Scene() {
    // Create root node
    rootKey = nodes.add(SceneNode{
        SceneNodeKey::null(), // parentKey
        {},                   // childrenKeys
        glm::mat4(1.0f),      // localTransform
        glm::mat4(1.0f),      // worldTransform
        false,                // dirty
        "root"                // name
    });
}

Scene::~Scene() = default;

SceneNodeKey Scene::addNode(SceneNodeKey parentKey, const std::string& name) {
    if (parentKey.isNull()) {
        parentKey = rootKey;
    }

    SceneNodeKey newNodeKey = nodes.add();
    SceneNode& newNode = nodes[newNodeKey];
    newNode.parentKey = parentKey;
    newNode.name = name;
    newNode.dirty = true; // New nodes are always dirty initially

    SceneNode& parentNode = nodes[parentKey];
    parentNode.childrenKeys.push_back(newNodeKey);

    if (!parentNode.dirty) {
        // Top of a dirty tree, so insert into minimal dirty set
        minimalDirtySet.insert(newNodeKey);
    }

    return newNodeKey;
}

void Scene::removeNode(SceneNodeKey nodeKey, bool removeAllDescendants)
{
    ASSERT(!nodeKey.isNull() && "Node key cannot be null");
    ASSERT(nodeKey != rootKey && "Cannot remove root node");

    SceneNode& node = nodes[nodeKey];

    SceneNodeKey parentKey = node.parentKey;
    SceneNode& parentNode = nodes[parentKey];

    if (removeAllDescendants) {
        // Remove all descendants recursively
        // Use a copy of childrenKeys to avoid modifying the vector while iterating
        std::vector<SceneNodeKey> childrenKeysCopy = node.childrenKeys;
        for (SceneNodeKey childKey : childrenKeysCopy) {
            removeNode(childKey, true);
        }
    }

    // Remove node from parent's childrenKeys
    auto& siblings = parentNode.childrenKeys;
    auto it = std::ranges::find(siblings, nodeKey);
    if (it != siblings.end()) {
        std::swap(*it, siblings.back());
        siblings.pop_back();
    }

    // Patch children to parent
    for (SceneNodeKey childKey : node.childrenKeys) {
        SceneNode& childNode = nodes[childKey];
        childNode.parentKey = parentKey;
        parentNode.childrenKeys.push_back(childKey);
        // Mark child as dirty since its parent is changing
        markDirty(childKey);
    }

    // Update internal state tracking
    minimalDirtySet.erase(nodeKey);
    changedNodes.erase(nodeKey);
    removedNodes.insert(nodeKey);

    // Remove node from slotmap
    nodes.remove(nodeKey);
}

void Scene::attachNode(SceneNodeKey nodeKey, SceneNodeKey newParentKey) {
    ASSERT(!nodeKey.isNull() && "Node key cannot be null");
    ASSERT(!newParentKey.isNull() && "New parent key cannot be null");
    ASSERT(nodeKey != rootKey && "Cannot attach root node to another parent");

    SceneNode& node = nodes[nodeKey];
    SceneNodeKey oldParentKey = node.parentKey;
    SceneNode& oldParent = nodes[oldParentKey];
    auto& siblings = oldParent.childrenKeys;
    // Remove nodeKey from siblings using swap-and-pop for efficiency
    auto it = std::ranges::find(siblings, nodeKey);
    if (it != siblings.end()) {
        std::swap(*it, siblings.back());
        siblings.pop_back();
    }
    node.parentKey = newParentKey;
    SceneNode& newParent = nodes[newParentKey];
    newParent.childrenKeys.push_back(nodeKey);

    // Mark the node and its descendants as dirty
    markDirty(nodeKey);
}

void Scene::detachNode(SceneNodeKey nodeKey)
{
    attachNode(nodeKey, rootKey);
}

SceneNodeKey Scene::getRootKey() const
{
    return rootKey;
}

SceneNodeKey Scene::getParentKey(SceneNodeKey nodeKey) const {
    return nodes.get<SceneNode>(nodeKey)->parentKey;
}

const std::vector<SceneNodeKey>& Scene::getChildrenKeys(SceneNodeKey nodeKey) const {
    return nodes.get<SceneNode>(nodeKey)->childrenKeys;
}

SceneNodeKey Scene::findNodeByName(const std::string &name) const
{
    for (const auto& [key, node] : nodes) {
        if (node.name == name) {
            return key;
        }
    }
    return SceneNodeKey::null(); // Not found
}

const SceneNode* Scene::getNode(SceneNodeKey nodeKey) const
{
    return nodes.get<SceneNode>(nodeKey);
}

SceneNode* Scene::getNode(SceneNodeKey nodeKey)
{
    return const_cast<SceneNode*>(static_cast<const Scene*>(this)->getNode(nodeKey));
}

glm::mat4 Scene::getLocalTransform(SceneNodeKey nodeKey) const
{
    return nodes[nodeKey].localTransform;
}

glm::mat4 Scene::getWorldTransform(SceneNodeKey nodeKey, bool updateIfDirty)
{
    SceneNode& node = nodes[nodeKey];
    if (updateIfDirty && node.dirty) {
        propagateTransformsTo(nodeKey);
    }
    return node.worldTransform;
}

glm::mat4 Scene::getWorldTransform(SceneNodeKey nodeKey, bool updateIfDirty) const
{
    const SceneNode& node = nodes[nodeKey];
    if (updateIfDirty && node.dirty) {
        ASSERT(false && 
               "This should not be called with updateIfDirty=true on const Scene, "
               "use non-const version to update transforms");
    }
    return node.worldTransform;
}

void Scene::setLocalTransform(SceneNodeKey nodeKey, const glm::mat4& local) {
    SceneNode& node = nodes[nodeKey];
    if (node.localTransform == local) {
        return; // No change, no need to dirty
    }
    node.localTransform = local;

    // Mark the node and all descendants as dirty
    // Skip updating dirty state if already dirty; covered by parent if so.
    // If node isn't dirty, parent also can't be dirty, so covered check 
    // isn't needed either.
    markDirty(nodeKey, true, true);
}

void Scene::setWorldTransform(SceneNodeKey nodeKey, const glm::mat4 &world)
{
    SceneNode& node = nodes[nodeKey];
    if (node.worldTransform == world && !node.dirty) {
        // No change, no need to dirty
        return;
    }

    // Ensure the parent world transform is up to date
    SceneNodeKey parentKey = node.parentKey;
    propagateTransformsTo(parentKey);

    // Compute the local transform based on the parent world
    // P * L = W -> L = P^-1 * W
    glm::mat4 parentWorld = parentKey.isNull() ? 
        glm::mat4(1.0f) : 
        nodes[parentKey].worldTransform;
    node.localTransform = glm::inverse(parentWorld) * world;

    // Update world transform and mark as changed
    if (node.worldTransform != world) {
        node.worldTransform = world;
        changedNodes.insert(nodeKey);
    }

    // Clear dirty state on this node and mark all descendants as dirty
    node.dirty = false;
    minimalDirtySet.erase(nodeKey);
    for (const auto& childKey : node.childrenKeys) {
        // We CAN'T skip update on already dirty children, as they will need
        // to be added to the minimal dirty set still. We can skip the covered check
        // though, since the parent node is clean.
        markDirty(childKey, false, true);
    }
}

void Scene::markDirty(SceneNodeKey nodeKey, 
    bool skipUpdateIfDirty, 
    bool skipCoveredCheck)
{
    SceneNode& node = nodes[nodeKey];
    if (skipUpdateIfDirty && node.dirty) {
        return;
    }

    markDirtyRecursive(nodeKey);

    // Add to minimal dirty set if not already covered by parent.
    if (skipCoveredCheck ||
        node.parentKey.isNull() ||
        !nodes[node.parentKey].dirty) 
    {
        minimalDirtySet.insert(nodeKey);
    }
}

void Scene::markDirtyRecursive(SceneNodeKey nodeKey) {
    SceneNode& node = nodes[nodeKey];
    if (node.dirty) {
        // Remove from minimal dirty set if already dirty; 
        // descendants are already covered by ancestor.
        minimalDirtySet.erase(nodeKey);
        return;
    }

    node.dirty = true;
    for (const auto& childKey : node.childrenKeys) {
        markDirtyRecursive(childKey);
    }
}

void Scene::propagateSubtree(SceneNodeKey nodeKey, const glm::mat4& parentWorld, SceneNodeKeySet& changedNodes) {
    SceneNode& node = nodes[nodeKey];
    glm::mat4 prevWorld = node.worldTransform;
    node.worldTransform = parentWorld * node.localTransform;
    node.dirty = false;
    if (node.worldTransform != prevWorld) {
        changedNodes.insert(nodeKey);
    }
    for (const auto& childKey : node.childrenKeys) {
        propagateSubtree(childKey, node.worldTransform, changedNodes);
    }
}

void Scene::propagateTransformsTo(SceneNodeKey targetKey)
{
    if (targetKey.isNull() || !nodes[targetKey].dirty) {
        return; // Nothing to propagate
    }

    // Propagate to parent first
    propagateTransformsTo(nodes[targetKey].parentKey);
    
    // The parents world transform is now up to date,
    // so we can use it to compute the targets world transform
    SceneNode& targetNode = nodes[targetKey];
    SceneNodeKey parentKey = targetNode.parentKey;
    glm::mat4 prevWorld = targetNode.worldTransform;
    glm::mat4 parentWorld = parentKey.isNull() ? 
        glm::mat4(1.0f) : 
        nodes[parentKey].worldTransform;
    targetNode.worldTransform = parentWorld * targetNode.localTransform;

    // If the world transform has changed, add to changed nodes
    if (targetNode.worldTransform != prevWorld) {
        changedNodes.insert(targetKey);
    }

    // Set dirty state to false for the target node
    targetNode.dirty = false;
    
    // Propagate down minimal dirty set state to children, 
    // since they're still dirty and need to be covered
    if (minimalDirtySet.erase(targetKey)) {
        for (const auto& childKey : targetNode.childrenKeys) {
            minimalDirtySet.insert(childKey);
        }
    }
}

void Scene::propagateTransforms() {
    // Propagate transforms for all minimal dirty roots
    for (SceneNodeKey dirtyKey : minimalDirtySet) {
        glm::mat4 parentWorld = glm::mat4(1.0f);
        SceneNodeKey parentKey = nodes[dirtyKey].parentKey;
        if (!parentKey.isNull()) {
            parentWorld = nodes[parentKey].worldTransform;
        }
        propagateSubtree(dirtyKey, parentWorld, changedNodes);
    }
    minimalDirtySet.clear();
}

void Scene::finalizeAndNotify() {
    propagateTransforms();
    // Notify observers of changed nodes
    for (auto* observer : observers) {
        ASSERT(observer && "Observer should not be null");
        if (!changedNodes.empty()) {
            observer->onSceneNodesChanged(changedNodes);
        }
        if (!removedNodes.empty()) {
            observer->onSceneNodesRemoved(removedNodes);
        }
    }
    changedNodes.clear();
}

// Observer registration implementations
void Scene::addObserver(ISceneObserver* observer) {
    if (observer && std::ranges::find(observers, observer) == observers.end()) {
        observers.push_back(observer);
    }
}

void Scene::removeObserver(ISceneObserver* observer) {
    auto [begin, end] = std::ranges::remove(observers, observer);
    if (begin != end) {
        observers.erase(begin, end);
    }
}

bool Scene::isNodeDirty(SceneNodeKey nodeKey) const
{
    return nodes[nodeKey].dirty;
}

const SceneNodeKeySet &Scene::getMinimalDirtySet() const
{
    return minimalDirtySet;
}

const SceneNodeKeySet& Scene::getChangedNodes() const {
    return changedNodes;
}

const SceneNodeKeySet& Scene::getRemovedNodes() const {
    return removedNodes;
}