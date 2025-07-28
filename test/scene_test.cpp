#include "scene/scene.h"
#include <gtest/gtest.h>
#include <string>

// Mock observer for testing
class MockSceneObserver : public ISceneObserver {
public:
    int notify_count = 0;
    int remove_notify_count = 0;
    SceneNodeKeySet lastChanged;
    SceneNodeKeySet lastRemoved;

    void onSceneNodesChanged(const SceneNodeKeySet& changedNodes) override {
        notify_count++;
        lastChanged = changedNodes;
    }
    void onSceneNodesRemoved(const SceneNodeKeySet& removedNodes) override {
        remove_notify_count++;
        lastRemoved = removedNodes;
    }
};

TEST(SceneTest, RootNodeExistsAndEmpty) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    EXPECT_NE(root, SceneNodeKey::null());
    const auto& children = scene.getChildrenKeys(root);
    EXPECT_TRUE(children.empty());
}

TEST(SceneTest, AddNodeAndHierarchy) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    SceneNodeKey grandchild = scene.addNode(child, "grandchild");

    // Check parent relationships
    EXPECT_EQ(scene.getParentKey(child), root);
    EXPECT_EQ(scene.getParentKey(grandchild), child);

    // Check children relationships
    const auto& rootChildren = scene.getChildrenKeys(root);
    ASSERT_EQ(rootChildren.size(), 1u);
    EXPECT_EQ(rootChildren[0], child);
    const auto& childChildren = scene.getChildrenKeys(child);
    ASSERT_EQ(childChildren.size(), 1u);
    EXPECT_EQ(childChildren[0], grandchild);
    const auto& grandchildChildren = scene.getChildrenKeys(grandchild);
    EXPECT_TRUE(grandchildChildren.empty());

    // Check name correctness
    const SceneNode* rootNode = scene.getNode(root);
    const SceneNode* childNode = scene.getNode(child);
    const SceneNode* grandchildNode = scene.getNode(grandchild);
    ASSERT_NE(rootNode, nullptr);
    ASSERT_NE(childNode, nullptr);
    ASSERT_NE(grandchildNode, nullptr);
    EXPECT_EQ(rootNode->name, "root");
    EXPECT_EQ(childNode->name, "child");
    EXPECT_EQ(grandchildNode->name, "grandchild");
}

TEST(SceneTest, AddNodeWithInvalidParentUsesRoot) {
    Scene scene{};
    SceneNodeKey invalidKey = SceneNodeKey::null();
    SceneNodeKey node = scene.addNode(invalidKey, "orphan");
    EXPECT_EQ(scene.getParentKey(node), scene.getRootKey());
    const auto& rootChildren = scene.getChildrenKeys(scene.getRootKey());
    EXPECT_EQ(rootChildren.size(), 1u);
    EXPECT_EQ(rootChildren[0], node);
    // Check name correctness
    const SceneNode* nodePtr = scene.getNode(node);
    ASSERT_NE(nodePtr, nullptr);
    EXPECT_EQ(nodePtr->name, "orphan");
}

TEST(SceneTest, GetNodeConstReturnsCorrectNode) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    const Scene& constScene = const_cast<const Scene&>(scene);
    const SceneNode* nodePtr = constScene.getNode(child);
    ASSERT_NE(nodePtr, nullptr);
    EXPECT_EQ(nodePtr->parentKey, root);
    EXPECT_TRUE(nodePtr->childrenKeys.empty());
    EXPECT_EQ(nodePtr->name, "child");
}

TEST(SceneTest, GetNodeNonConstReturnsCorrectNode) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    SceneNode* nodePtr = scene.getNode(child);
    ASSERT_NE(nodePtr, nullptr);
    EXPECT_EQ(nodePtr->parentKey, root);
    EXPECT_TRUE(nodePtr->childrenKeys.empty());
    EXPECT_EQ(nodePtr->name, "child");
}

TEST(SceneTest, MultipleChildren) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child1 = scene.addNode(root, "child1");
    SceneNodeKey child2 = scene.addNode(root, "child2");
    const auto& rootChildren = scene.getChildrenKeys(root);
    EXPECT_EQ(rootChildren.size(), 2u);
    EXPECT_TRUE(
        (rootChildren[0] == child1 && rootChildren[1] == child2) ||
        (rootChildren[0] == child2 && rootChildren[1] == child1)
    );
    // Check name correctness
    const SceneNode* child1Node = scene.getNode(child1);
    const SceneNode* child2Node = scene.getNode(child2);
    ASSERT_NE(child1Node, nullptr);
    ASSERT_NE(child2Node, nullptr);
    EXPECT_EQ(child1Node->name, "child1");
    EXPECT_EQ(child2Node->name, "child2");
}

TEST(SceneTest, DirtyFlagAndMinimalDirtySetSingleNode) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    SceneNodeKey grandchild = scene.addNode(child, "grandchild");

    // Initially, all nodes except root should be dirty (from addNode)
    EXPECT_TRUE(scene.getNode(child)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild)->dirty);
    EXPECT_FALSE(scene.getNode(root)->dirty);

    // Flush dirty state for further tests
    scene.finalizeAndNotify();

    // Set local transform on child, should dirty child and grandchild
    scene.setLocalTransform(child, glm::mat4(2.0f));
    EXPECT_TRUE(scene.getNode(child)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild)->dirty);
    EXPECT_FALSE(scene.getNode(root)->dirty);

    // Minimal dirty set should contain only child
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(child));
    // Node in minimal dirty set should be dirty
    for (auto key : scene.getMinimalDirtySet()) {
        EXPECT_TRUE(scene.getNode(key)->dirty);
    }
}

TEST(SceneTest, DirtyFlagCoveredByParent) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    SceneNodeKey grandchild = scene.addNode(child, "grandchild");

    // Flush dirty state after node creation
    scene.finalizeAndNotify();

    // Set local transform on root, should dirty all
    scene.setLocalTransform(root, glm::mat4(2.0f));
    EXPECT_TRUE(scene.getNode(root)->dirty);
    EXPECT_TRUE(scene.getNode(child)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild)->dirty);

    // Minimal dirty set should contain only root
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(root));
    for (auto key : scene.getMinimalDirtySet()) {
        EXPECT_TRUE(scene.getNode(key)->dirty);
    }

    // Now set local transform on grandchild, should not add grandchild to minimal dirty set
    scene.setLocalTransform(grandchild, glm::mat4(3.0f));
    EXPECT_TRUE(scene.getNode(grandchild)->dirty);
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(root));
    for (auto key : scene.getMinimalDirtySet()) {
        EXPECT_TRUE(scene.getNode(key)->dirty);
    }
}

TEST(SceneTest, AddNodeMarksDirtyAndMinimalDirtySet) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    EXPECT_TRUE(scene.getNode(child)->dirty);
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(child));
    for (auto key : scene.getMinimalDirtySet()) {
        EXPECT_TRUE(scene.getNode(key)->dirty);
    }
}

TEST(SceneTest, PropagateTransformsTo_RemovesMinimalDirtySetIfSingleChild) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();

    SceneNodeKey child = scene.addNode(root, "child");
    SceneNodeKey grandchild = scene.addNode(child, "grandchild");

    // Flush dirty state after node creation
    scene.finalizeAndNotify();

    // Use setLocalTransform to dirty only child and grandchild
    scene.setLocalTransform(child, glm::mat4(2.0f));

    // Propagate transforms to grandchild
    scene.propagateTransformsTo(grandchild);

    // After propagation, minimal_dirty_set should be empty (child had only one child)
    EXPECT_TRUE(scene.getMinimalDirtySet().empty());
}

TEST(SceneTest, PropagateTransformsTo_ChainLeavesDescendantsDirty) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    SceneNodeKey grandchild = scene.addNode(child, "grandchild");
    SceneNodeKey greatgrandchild = scene.addNode(grandchild, "greatgrandchild");

    scene.finalizeAndNotify();

    // Dirty the whole chain
    scene.setLocalTransform(root, glm::mat4(2.0f));

    // Propagate only to grandchild
    scene.propagateTransformsTo(grandchild);

    // grandchild should not be dirty, but greatgrandchild should still be dirty
    EXPECT_FALSE(scene.getNode(grandchild)->dirty);
    EXPECT_TRUE(scene.getNode(greatgrandchild)->dirty);

    // Minimal dirty set should contain greatgrandchild only
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(greatgrandchild));
    for (auto key : scene.getMinimalDirtySet()) {
        EXPECT_TRUE(scene.getNode(key)->dirty);
    }

    // changedNodes should include grandchild
    EXPECT_TRUE(scene.getChangedNodes().count(grandchild));
}

TEST(SceneTest, PropagateTransformsTo_MultiChildDirtySetBehavior) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child1 = scene.addNode(root, "child1");
    SceneNodeKey child2 = scene.addNode(root, "child2");
    SceneNodeKey child3 = scene.addNode(root, "child3");

    scene.finalizeAndNotify();

    // Dirty all children
    scene.setLocalTransform(root, glm::mat4(2.0f));

    // Propagate to child2 only
    scene.propagateTransformsTo(child2);

    // child2 should not be dirty, but child1 and child3 should remain dirty
    EXPECT_FALSE(scene.getNode(child2)->dirty);
    EXPECT_TRUE(scene.getNode(child1)->dirty);
    EXPECT_TRUE(scene.getNode(child3)->dirty);

    // Minimal dirty set should contain child1 and child3 only
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 2u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(child1));
    EXPECT_TRUE(scene.getMinimalDirtySet().count(child3));
    EXPECT_FALSE(scene.getMinimalDirtySet().count(root));
    for (auto key : scene.getMinimalDirtySet()) {
        EXPECT_TRUE(scene.getNode(key)->dirty);
    }

    // changedNodes should include child2
    EXPECT_TRUE(scene.getChangedNodes().count(child2));
}

TEST(SceneTest, PropagateTransformsTo_ChangedNodesAreCorrect) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    SceneNodeKey grandchild = scene.addNode(child, "grandchild");

    scene.finalizeAndNotify();

    // Dirty all
    scene.setLocalTransform(root, glm::mat4(2.0f));
    scene.setLocalTransform(child, glm::mat4(3.0f));

    // Propagate to grandchild
    scene.propagateTransformsTo(grandchild);

    // changedNodes should include grandchild, child, and root
    EXPECT_TRUE(scene.getChangedNodes().count(root));
    EXPECT_TRUE(scene.getChangedNodes().count(child));
    EXPECT_TRUE(scene.getChangedNodes().count(grandchild));
}

TEST(SceneTest, PropagateTransforms_ClearsDirtyFlagsAndMinimalDirtySet) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    SceneNodeKey grandchild = scene.addNode(child, "grandchild");

    // Flush dirty state after node creation
    scene.finalizeAndNotify();

    // Use setLocalTransform to dirty all nodes
    scene.setLocalTransform(root, glm::mat4(2.0f));

    scene.propagateTransforms();

    // All nodes should be not dirty
    EXPECT_FALSE(scene.getNode(root)->dirty);
    EXPECT_FALSE(scene.getNode(child)->dirty);
    EXPECT_FALSE(scene.getNode(grandchild)->dirty);
    // Minimal dirty set should be cleared
    EXPECT_TRUE(scene.getMinimalDirtySet().empty());
}

TEST(SceneTest, FinalizeForRendering_ClearsChangedNodes) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");

    // Flush dirty state after node creation
    scene.finalizeAndNotify();

    // Use setLocalTransform to dirty child
    scene.setLocalTransform(child, glm::mat4(2.0f));

    // After finalizeAndNotify, changedNodes should be cleared
    scene.finalizeAndNotify();
    EXPECT_TRUE(scene.getChangedNodes().empty());
    // Child should be not dirty
    EXPECT_FALSE(scene.getNode(child)->dirty);
    // Minimal dirty set should be cleared
    EXPECT_TRUE(scene.getMinimalDirtySet().empty());
}

TEST(SceneTest, GetLocalTransformReturnsCorrectValue) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    glm::mat4 custom = glm::mat4(2.0f);
    scene.setLocalTransform(root, custom);
    EXPECT_EQ(scene.getLocalTransform(root), custom);
}

TEST(SceneTest, GetWorldTransformReturnsCorrectValueAndUpdatesIfDirty) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");

    // Flush dirty state after node creation
    scene.finalizeAndNotify();

    glm::mat4 rootLocal = glm::mat4(2.0f);
    glm::mat4 childLocal = glm::mat4(3.0f);
    scene.setLocalTransform(root, rootLocal);
    scene.setLocalTransform(child, childLocal);

    // Both should be dirty
    EXPECT_TRUE(scene.isNodeDirty(root));
    EXPECT_TRUE(scene.isNodeDirty(child));
    
    // getWorldTransform should update and return correct value
    glm::mat4 expectedChildWorld = rootLocal * childLocal;
    EXPECT_EQ(scene.getWorldTransform(child, true), expectedChildWorld);

    // After call, dirty flags should be cleared
    EXPECT_FALSE(scene.isNodeDirty(root));
    EXPECT_FALSE(scene.isNodeDirty(child));
}

TEST(SceneTest, IsNodeDirtyReturnsCorrectState) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    EXPECT_FALSE(scene.isNodeDirty(root));
    scene.getNode(root)->dirty = true;
    EXPECT_TRUE(scene.isNodeDirty(root));
}

TEST(SceneTest, ObserverReceivesTransformChangeNotification) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    
    // Flush dirty state after node creation
    scene.finalizeAndNotify();

    MockSceneObserver observer;
    scene.addObserver(&observer);

    // Use setLocalTransform to dirty child
    scene.setLocalTransform(child, glm::mat4(2.0f));

    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 1);
    EXPECT_TRUE(observer.lastChanged.count(child));
    EXPECT_FALSE(observer.lastChanged.count(root));
}

TEST(SceneTest, ObserverNotNotifiedAfterRemoval) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    
    // Flush dirty state after node creation
    scene.finalizeAndNotify();

    MockSceneObserver observer;
    scene.addObserver(&observer);

    // Use setLocalTransform to dirty child
    scene.setLocalTransform(child, glm::mat4(2.0f));

    scene.removeObserver(&observer);
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 0);
}

TEST(SceneTest, MultipleObserversAreNotified) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    
    // Flush dirty state after node creation
    scene.finalizeAndNotify();
    
    MockSceneObserver observer1, observer2;
    scene.addObserver(&observer1);
    scene.addObserver(&observer2);

    // Use setLocalTransform to dirty child
    scene.setLocalTransform(child, glm::mat4(2.0f));

    scene.finalizeAndNotify();
    EXPECT_EQ(observer1.notify_count, 1);
    EXPECT_EQ(observer2.notify_count, 1);
    EXPECT_TRUE(observer1.lastChanged.count(child));
    EXPECT_TRUE(observer2.lastChanged.count(child));
}

TEST(SceneTest, ComplexGraphTransformPropagationAndNotification) {
    Scene scene{};
    
    // Build a complex graph:
    // root
    // ├── child1
    // │   ├── grandchild1
    // │   └── grandchild2
    // └── child2
    //     └── grandchild3
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child1 = scene.addNode(root, "child1");
    SceneNodeKey child2 = scene.addNode(root, "child2");
    SceneNodeKey grandchild1 = scene.addNode(child1, "grandchild1");
    SceneNodeKey grandchild2 = scene.addNode(child1, "grandchild2");
    SceneNodeKey grandchild3 = scene.addNode(child2, "grandchild3");
    
    // Flush dirty state after node creation
    scene.finalizeAndNotify();

    MockSceneObserver observer;
    scene.addObserver(&observer);

    // Set local transform on child1, should dirty child1, grandchild1, grandchild2
    scene.setLocalTransform(child1, glm::mat4(2.0f));
    EXPECT_TRUE(scene.getNode(child1)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild1)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild2)->dirty);
    EXPECT_FALSE(scene.getNode(child2)->dirty);
    EXPECT_FALSE(scene.getNode(grandchild3)->dirty);

    // Only child1 should be in minimal dirty set
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(child1));
    for (auto key : scene.getMinimalDirtySet()) {
        EXPECT_TRUE(scene.getNode(key)->dirty);
    }

    // Finalize and notify, observer should get child1, grandchild1, grandchild2
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 1);
    EXPECT_TRUE(observer.lastChanged.count(child1));
    EXPECT_TRUE(observer.lastChanged.count(grandchild1));
    EXPECT_TRUE(observer.lastChanged.count(grandchild2));
    EXPECT_FALSE(observer.lastChanged.count(child2));
    EXPECT_FALSE(observer.lastChanged.count(grandchild3));

    // Now set local transform on root, which should dirty all nodes
    scene.setLocalTransform(root, glm::mat4(3.0f));
    EXPECT_TRUE(scene.getNode(root)->dirty);
    EXPECT_TRUE(scene.getNode(child1)->dirty);
    EXPECT_TRUE(scene.getNode(child2)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild1)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild2)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild3)->dirty);
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(root));
    for (auto key : scene.getMinimalDirtySet()) {
        EXPECT_TRUE(scene.getNode(key)->dirty);
    }

    // Finalize and notify, observer should get all nodes
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 2);
    EXPECT_TRUE(observer.lastChanged.count(root));
    EXPECT_TRUE(observer.lastChanged.count(child1));
    EXPECT_TRUE(observer.lastChanged.count(child2));
    EXPECT_TRUE(observer.lastChanged.count(grandchild1));
    EXPECT_TRUE(observer.lastChanged.count(grandchild2));
    EXPECT_TRUE(observer.lastChanged.count(grandchild3));

    // Now set local transform on grandchild3 only
    scene.setLocalTransform(grandchild3, glm::mat4(4.0f));
    EXPECT_TRUE(scene.getNode(grandchild3)->dirty);
    EXPECT_FALSE(scene.getNode(root)->dirty);
    EXPECT_FALSE(scene.getNode(child1)->dirty);
    EXPECT_FALSE(scene.getNode(child2)->dirty);
    EXPECT_FALSE(scene.getNode(grandchild1)->dirty);
    EXPECT_FALSE(scene.getNode(grandchild2)->dirty);
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(grandchild3));
    for (auto key : scene.getMinimalDirtySet()) {
        EXPECT_TRUE(scene.getNode(key)->dirty);
    }

    // Finalize and notify, observer should get only grandchild3
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 3);
    EXPECT_TRUE(observer.lastChanged.count(grandchild3));
    EXPECT_FALSE(observer.lastChanged.count(root));
    EXPECT_FALSE(observer.lastChanged.count(child1));
    EXPECT_FALSE(observer.lastChanged.count(child2));
    EXPECT_FALSE(observer.lastChanged.count(grandchild1));
    EXPECT_FALSE(observer.lastChanged.count(grandchild2));
}

TEST(SceneTest, AttachNodeReparentsAndDirtyState) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey a = scene.addNode(root, "A");
    SceneNodeKey b = scene.addNode(root, "B");
    SceneNodeKey c = scene.addNode(a, "C");
    scene.finalizeAndNotify();

    // Attach C under B
    scene.attachNode(c, b);
    EXPECT_EQ(scene.getParentKey(c), b);
    const auto& bChildren = scene.getChildrenKeys(b);
    EXPECT_TRUE(std::find(bChildren.begin(), bChildren.end(), c) != bChildren.end());
    // A should no longer have C as child
    const auto& aChildren = scene.getChildrenKeys(a);
    EXPECT_TRUE(std::find(aChildren.begin(), aChildren.end(), c) == aChildren.end());
    // C should be dirty after reparent
    EXPECT_TRUE(scene.getNode(c)->dirty);
    // Minimal dirty set should contain C
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(c));
}

TEST(SceneTest, RemoveNodePatchesChildrenAndDirtyState) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey a = scene.addNode(root, "A");
    SceneNodeKey b = scene.addNode(a, "B");
    SceneNodeKey c = scene.addNode(a, "C");
    scene.finalizeAndNotify();

    MockSceneObserver observer;
    scene.addObserver(&observer);

    // Remove A, B and C should be patched to root
    scene.removeNode(a);
    EXPECT_EQ(scene.getParentKey(b), root);
    EXPECT_EQ(scene.getParentKey(c), root);
    const auto& rootChildren = scene.getChildrenKeys(root);
    EXPECT_TRUE(std::find(rootChildren.begin(), rootChildren.end(), b) != rootChildren.end());
    EXPECT_TRUE(std::find(rootChildren.begin(), rootChildren.end(), c) != rootChildren.end());
    // B and C should be dirty after reparent
    EXPECT_TRUE(scene.getNode(b)->dirty);
    EXPECT_TRUE(scene.getNode(c)->dirty);
    // Minimal dirty set should contain B and C
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 2u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(b));
    EXPECT_TRUE(scene.getMinimalDirtySet().count(c));
    // After finalizeAndNotify, observer should be notified of removal
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.remove_notify_count, 1);
    // Removed nodes set should contain A
    EXPECT_TRUE(observer.lastRemoved.count(a));
}

TEST(SceneTest, RemoveLeafNode) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey a = scene.addNode(root, "A");
    scene.finalizeAndNotify();
    MockSceneObserver observer;
    scene.addObserver(&observer);
    // Remove leaf node
    scene.removeNode(a);
    EXPECT_TRUE(scene.getChildrenKeys(root).empty());
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 0u);
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.remove_notify_count, 1);
    EXPECT_TRUE(observer.lastRemoved.count(a));
}

TEST(SceneTest, AttachNodeToRootAndDirtyState) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey a = scene.addNode(root, "A");
    SceneNodeKey b = scene.addNode(a, "B");
    scene.finalizeAndNotify();
    // Attach B to root
    scene.attachNode(b, root);
    EXPECT_EQ(scene.getParentKey(b), root);
    const auto& rootChildren = scene.getChildrenKeys(root);
    EXPECT_TRUE(std::find(rootChildren.begin(), rootChildren.end(), b) != rootChildren.end());
    // B should be dirty
    EXPECT_TRUE(scene.getNode(b)->dirty);
    // Minimal dirty set should contain B
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(b));
}

TEST(SceneTest, RemoveNodeWithDescendants_RemovesAll) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey a = scene.addNode(root, "A");
    SceneNodeKey b = scene.addNode(a, "B");
    SceneNodeKey c = scene.addNode(a, "C");
    SceneNodeKey d = scene.addNode(b, "D");
    scene.finalizeAndNotify();
    MockSceneObserver observer;
    scene.addObserver(&observer);
    // Remove A and all descendants
    scene.removeNode(a, true);
    // Root should have no children
    EXPECT_TRUE(scene.getChildrenKeys(root).empty());
    scene.finalizeAndNotify();
    // Observer should be notified of all removals
    EXPECT_EQ(observer.remove_notify_count, 1);
    EXPECT_TRUE(observer.lastRemoved.count(a));
    EXPECT_TRUE(observer.lastRemoved.count(b));
    EXPECT_TRUE(observer.lastRemoved.count(c));
    EXPECT_TRUE(observer.lastRemoved.count(d));
}

TEST(SceneTest, SetWorldTransformUpdatesLocalAndWorld) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    scene.finalizeAndNotify();

    MockSceneObserver observer;
    scene.addObserver(&observer);

    glm::mat4 rootLocal = glm::mat4(2.0f);
    scene.setLocalTransform(root, rootLocal);

    glm::mat4 desiredWorld = glm::mat4(5.0f);
    scene.setWorldTransform(child, desiredWorld);
    
    // Child's world transform should be set
    EXPECT_EQ(scene.getNode(child)->worldTransform, desiredWorld);
    // Child's local transform should be parent^-1 * world
    glm::mat4 expectedLocal = glm::inverse(rootLocal) * desiredWorld;
    EXPECT_EQ(scene.getNode(child)->localTransform, expectedLocal);
    // Child should not be dirty
    EXPECT_FALSE(scene.getNode(child)->dirty);
    // Minimal dirty set should not contain child
    EXPECT_FALSE(scene.getMinimalDirtySet().count(child));
    // Changed nodes should include child
    EXPECT_TRUE(scene.getChangedNodes().count(child));
    
    // Observer should be notified of the change
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 1);
    EXPECT_TRUE(observer.lastChanged.count(child));
}

TEST(SceneTest, SetWorldTransformMarksDescendantsDirty) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    SceneNodeKey grandchild1 = scene.addNode(child, "grandchild1");
    SceneNodeKey grandchild2 = scene.addNode(child, "grandchild2");
    scene.finalizeAndNotify();

    MockSceneObserver observer;
    scene.addObserver(&observer);

    glm::mat4 desiredWorld = glm::mat4(3.0f);
    scene.setWorldTransform(child, desiredWorld);
    
    // Child should not be dirty
    EXPECT_FALSE(scene.getNode(child)->dirty);
    // Grandchild should be dirty
    EXPECT_TRUE(scene.getNode(grandchild1)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild2)->dirty);
    // Minimal dirty set should contain grandchild
    EXPECT_TRUE(scene.getMinimalDirtySet().count(grandchild1));
    EXPECT_TRUE(scene.getMinimalDirtySet().count(grandchild2));
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 2u);
    // Changed nodes should include child
    EXPECT_TRUE(scene.getChangedNodes().count(child));
    // Observer should be notified of the change
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 1);
    // Observer should have child and grandchildren in lastChanged
    EXPECT_FALSE(observer.lastChanged.count(root));
    EXPECT_TRUE(observer.lastChanged.count(child));
    EXPECT_TRUE(observer.lastChanged.count(grandchild1));
    EXPECT_TRUE(observer.lastChanged.count(grandchild2));
    EXPECT_EQ(observer.lastChanged.size(), 3u);
}

TEST(SceneTest, SetWorldTransformWithDirtyParent) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    scene.finalizeAndNotify();

    MockSceneObserver observer;
    scene.addObserver(&observer);

    // Dirty the parent
    scene.setLocalTransform(root, glm::mat4(4.0f));
    EXPECT_TRUE(scene.getNode(root)->dirty);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(root));

    glm::mat4 desiredWorld = glm::mat4(7.0f);
    scene.setWorldTransform(child, desiredWorld);
    
    // Child's world transform should be set
    EXPECT_EQ(scene.getNode(child)->worldTransform, desiredWorld);
    // Child should NOT be dirty
    EXPECT_FALSE(scene.getNode(child)->dirty);
    // Minimal dirty set should be empty (propagated from parent)
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 0u);
    // Changed nodes should include root and child (propogated by setWorldTransform)
    EXPECT_TRUE(scene.getChangedNodes().count(root));
    EXPECT_TRUE(scene.getChangedNodes().count(child));
    EXPECT_EQ(scene.getChangedNodes().size(), 2u);
    // Observer should be notified of the change
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 1);
    // Observer should have root and child in last_changed
    EXPECT_TRUE(observer.lastChanged.count(root));
    EXPECT_TRUE(observer.lastChanged.count(child));
    EXPECT_EQ(observer.lastChanged.size(), 2u);
}

TEST(SceneTest, ComplexGraphSetWorldTransformPropagationAndDirtyState) {
    Scene scene{};
    // Build a complex graph:
    // root
    // ├── child1
    // │   ├── grandchild1
    // │   └── grandchild2
    // └── child2
    //     └── grandchild3
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child1 = scene.addNode(root, "child1");
    SceneNodeKey child2 = scene.addNode(root, "child2");
    SceneNodeKey grandchild1 = scene.addNode(child1, "grandchild1");
    SceneNodeKey grandchild2 = scene.addNode(child1, "grandchild2");
    SceneNodeKey grandchild3 = scene.addNode(child2, "grandchild3");
    scene.finalizeAndNotify();

    MockSceneObserver observer;
    scene.addObserver(&observer);

    // Stage 1: Set world transform on child1
    glm::mat4 child1World = glm::mat4(10.0f);
    scene.setWorldTransform(child1, child1World);
    // child1 and ancestors should be clean
    EXPECT_FALSE(scene.getNode(child1)->dirty);
    EXPECT_FALSE(scene.getNode(root)->dirty);
    // Descendants dirty
    EXPECT_TRUE(scene.getNode(grandchild1)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild2)->dirty);
    // Sibling and its descendant clean
    EXPECT_FALSE(scene.getNode(child2)->dirty);
    EXPECT_FALSE(scene.getNode(grandchild3)->dirty);
    // Minimal dirty set: grandchild1, grandchild2
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 2u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(grandchild1));
    EXPECT_TRUE(scene.getMinimalDirtySet().count(grandchild2));
    // Changed nodes should include child1
    EXPECT_TRUE(scene.getChangedNodes().count(child1));
    EXPECT_EQ(scene.getChangedNodes().size(), 1u);

    // Stage 2: Finalize and notify, observer should get child1, grandchild1, grandchild2
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 1);
    EXPECT_TRUE(observer.lastChanged.count(child1));
    EXPECT_TRUE(observer.lastChanged.count(grandchild1));
    EXPECT_TRUE(observer.lastChanged.count(grandchild2));
    EXPECT_FALSE(observer.lastChanged.count(child2));
    EXPECT_FALSE(observer.lastChanged.count(grandchild3));
    EXPECT_EQ(observer.lastChanged.size(), 3u);

    // Stage 3: Set world transform on root
    glm::mat4 rootWorld = glm::mat4(20.0f);
    scene.setWorldTransform(root, rootWorld);
    // root clean
    EXPECT_FALSE(scene.getNode(root)->dirty);
    // All descendants dirty
    EXPECT_TRUE(scene.getNode(child1)->dirty);
    EXPECT_TRUE(scene.getNode(child2)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild1)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild2)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild3)->dirty);
    // Minimal dirty set: child1, child2
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 2u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(child1));
    EXPECT_TRUE(scene.getMinimalDirtySet().count(child2));
    // Changed nodes should include root
    EXPECT_TRUE(scene.getChangedNodes().count(root));
    EXPECT_EQ(scene.getChangedNodes().size(), 1u);

    // Stage 4: Finalize and notify, observer should get root, child1, child2, grandchild1, grandchild2, grandchild3
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 2);
    EXPECT_TRUE(observer.lastChanged.count(root));
    EXPECT_TRUE(observer.lastChanged.count(child1));
    EXPECT_TRUE(observer.lastChanged.count(child2));
    EXPECT_TRUE(observer.lastChanged.count(grandchild1));
    EXPECT_TRUE(observer.lastChanged.count(grandchild2));
    EXPECT_TRUE(observer.lastChanged.count(grandchild3));
    EXPECT_EQ(observer.lastChanged.size(), 6u);

    // Stage 5: Set world transform on root then on grandchild3
    rootWorld = glm::mat4(30.0f);
    scene.setWorldTransform(root, rootWorld);
    // root clean
    glm::mat4 grandchild3World = glm::mat4(40.0f);
    scene.setWorldTransform(grandchild3, grandchild3World);
    // grandchild3 and ancestors clean
    EXPECT_FALSE(scene.getNode(grandchild3)->dirty);
    EXPECT_FALSE(scene.getNode(child2)->dirty);
    EXPECT_FALSE(scene.getNode(root)->dirty);
    // Descendants (none) would be dirty if present
    // Siblings retain dirty state
    EXPECT_TRUE(scene.getNode(child1)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild1)->dirty);
    EXPECT_TRUE(scene.getNode(grandchild2)->dirty);
    // Minimal dirty set: child1
    EXPECT_EQ(scene.getMinimalDirtySet().size(), 1u);
    EXPECT_TRUE(scene.getMinimalDirtySet().count(child1));
    // Changed nodes should include root, child2, and grandchild3
    EXPECT_TRUE(scene.getChangedNodes().count(root));
    EXPECT_TRUE(scene.getChangedNodes().count(child2));
    EXPECT_TRUE(scene.getChangedNodes().count(grandchild3));
    EXPECT_EQ(scene.getChangedNodes().size(), 3u);

    // Stage 6: Finalize and notify, observer should be notified of the change
    scene.finalizeAndNotify();
    EXPECT_EQ(observer.notify_count, 3);
    // Observer should observe all nodes changed
    EXPECT_TRUE(observer.lastChanged.count(root));
    EXPECT_TRUE(observer.lastChanged.count(child1));
    EXPECT_TRUE(observer.lastChanged.count(child2));
    EXPECT_TRUE(observer.lastChanged.count(grandchild1));
    EXPECT_TRUE(observer.lastChanged.count(grandchild2));
    EXPECT_TRUE(observer.lastChanged.count(grandchild3));
    EXPECT_EQ(observer.lastChanged.size(), 6u);
}

TEST(SceneTest, FindNodeByName) {
    Scene scene{};
    SceneNodeKey root = scene.getRootKey();
    SceneNodeKey child = scene.addNode(root, "child");
    SceneNodeKey grandchild = scene.addNode(child, "grandchild");

    // Should find root, child, grandchild by name
    EXPECT_EQ(scene.findNodeByName("root"), root);
    EXPECT_EQ(scene.findNodeByName("child"), child);
    EXPECT_EQ(scene.findNodeByName("grandchild"), grandchild);

    // Should not find non-existent node
    EXPECT_EQ(scene.findNodeByName("not_a_node"), SceneNodeKey::null());
}
