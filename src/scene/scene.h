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
    SceneNodeKey parentKey;
    std::vector<SceneNodeKey> childrenKeys;
    glm::mat4 localTransform{1.0f};
    glm::mat4 worldTransform{1.0f};
    bool dirty = false;
    std::string name;
};

class ISceneObserver {
public:
    virtual ~ISceneObserver() = default;
    virtual void onSceneNodesChanged(const SceneNodeKeySet& changedNodes) = 0;
    virtual void onSceneNodesRemoved(const SceneNodeKeySet& removedNodes) = 0;
};

class Scene {
public:
    Scene();
    ~Scene();

    SceneNodeKey addNode(
        SceneNodeKey parentKey = SceneNodeKey::null(),
        const std::string& name = ""
    );
    void removeNode(SceneNodeKey nodeKey, bool removeAllDescendants = false);

    /**
     * Attaches an existing node to a new parent in the scene graph.
     * Removes the node from its current parent and adds it to the new parent's children.
     * Marks the node and its descendants as dirty for transform propagation.
     */
    void attachNode(SceneNodeKey nodeKey, SceneNodeKey newParentKey);
    /**
     * Detaches a node from its parent, and attaches it to the root node of the tree.
     * Marks the node and its descendants as dirty for transform propagation.
     */
    void detachNode(SceneNodeKey nodeKey);

    SceneNodeKey getRootKey() const;
    SceneNodeKey getParentKey(SceneNodeKey nodeKey) const;
    const std::vector<SceneNodeKey>& getChildrenKeys(SceneNodeKey nodeKey) const;
    SceneNodeKey findNodeByName(const std::string& name) const;
    const SceneNode* getNode(SceneNodeKey nodeKey) const;
    SceneNode* getNode(SceneNodeKey nodeKey);

    /**
     * Returns the local transform of the specified node.
     * Does not update or propagate transforms; returns the stored value.
     */
    glm::mat4 getLocalTransform(SceneNodeKey nodeKey) const;

    /**
     * Returns the world transform of the specified node.
     * If updateIfDirty is true (default), propagates transforms for the node 
     * and its ancestors if dirty, ensuring the returned transform is up to date.
     * If false, returns the cached world transform (may be stale).
     */
    glm::mat4 getWorldTransform(SceneNodeKey nodeKey, bool updateIfDirty = true);

    /**
     * Const version: Returns the cached world transform of the specified node.
     * If updateIfDirty is true and the node is dirty, triggers an assert (cannot
     * update in const context).
     */
    glm::mat4 getWorldTransform(SceneNodeKey nodeKey, bool updateIfDirty = true) const;

    /**
     * Sets the local transform of the specified node.
     * Marks the node and all descendants as dirty for transform propagation.
     * Updates the minimal dirty set accordingly.
     */
    void setLocalTransform(SceneNodeKey nodeKey, const glm::mat4& local);

    /**
     * Sets both the world transform and the local transform of the node.
     * Computes and sets the local transform so that parent_world * local = world.
     * Propagates transforms up to the parent if dirty before computing.
     * Clears the dirty flag on the node itself (since its world transform is set).
     * Marks all descendants as dirty for propagation.
     */
    void setWorldTransform(SceneNodeKey nodeKey, const glm::mat4& world);

    /**
     * Propagates transforms from the nearest dirty ancestor (or itself) down to the
     * specified target node, updating only the necessary chain. Stops as soon as the
     * target node is up to date and no longer dirty. Updates the minimal dirty set
     * accordingly.
     */
    void propagateTransformsTo(SceneNodeKey targetKey);

    /**
     * Propagates local-to-world transforms for all dirty nodes and their descendants.
     * Clears the minimal dirty set and updates the changed nodes set.
     */
    void propagateTransforms();

    /**
     * Performs final transform propagation.
     * Notifies all registered observers of node changes.
     * Clears the internal changed node set after nofitication.
     */
    void finalizeAndNotify();

    // Observer registration
    void addObserver(ISceneObserver* observer);
    void removeObserver(ISceneObserver* observer);

    bool isNodeDirty(SceneNodeKey nodeKey) const;

    // For testing and debugging: expose internal state
    const SceneNodeKeySet& getMinimalDirtySet() const;
    const SceneNodeKeySet& getChangedNodes() const;
    const SceneNodeKeySet& getRemovedNodes() const;

private:
    SlotMap<SceneNode> nodes;
    SceneNodeKey rootKey;

    // Set of top-most dirty nodes that need transform propagation.
    SceneNodeKeySet minimalDirtySet;

    // Set of nodes that have changed since the last frame. Used for observer
    // notifications.
    SceneNodeKeySet changedNodes;

    // Set of nodes that have been removed since the last frame. Used for observer
    // notifications.
    SceneNodeKeySet removedNodes;

    // Observers for transform change notifications
    std::vector<ISceneObserver*> observers;

    /**
     * Marks the node and all its descendants as dirty. Also updates the minimal
     * dirty set such that only the top-most dirty ancestor is kept.
     * @param skipUpdateIfDirty If true, skip dirty propagation, and updating 
     * of the minimal dirty set, if the node is already dirty. Useful optimization
     * when we know that no changes will be made when the node is dirty.
     * @param skipCoveredCheck If true, skips checking if the node is already 
     * covered by a parent in the minimal dirty set. Useful optimization when we
     * know the node is not covered.
     */
    void markDirty(
        SceneNodeKey nodeKey, 
        bool skipUpdateIfDirty=false,
        bool skipCoveredCheck=false);

    // Helper for markDirty: Recursively marks nodes as dirty and removes
    // them from the minimal dirty set.
    void markDirtyRecursive(SceneNodeKey nodeKey);

    // Helper: propagate transforms down a subtree.
    void propagateSubtree(
        SceneNodeKey nodeKey,
        const glm::mat4& parentWorld,
        SceneNodeKeySet& changedNodes);
    // ...existing code...
};
