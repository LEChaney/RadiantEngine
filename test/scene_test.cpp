#include "scene/scene.h"
#include <gtest/gtest.h>
#include <string>

// Mock observer for testing
class MockSceneObserver : public ISceneObserver {
public:
    int notify_count = 0;
    SceneNodeKeySet last_changed;

    void on_scene_nodes_changed(const SceneNodeKeySet& changed_nodes) override {
        ++notify_count;
        last_changed = changed_nodes;
    }
};

TEST(SceneTest, RootNodeExistsAndEmpty) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    EXPECT_NE(root, SceneNodeKey::null());
    const auto& children = scene.get_children_keys(root);
    EXPECT_TRUE(children.empty());
}

TEST(SceneTest, AddNodeAndHierarchy) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    SceneNodeKey grandchild = scene.add_node(child, "grandchild");

    // Check parent relationships
    EXPECT_EQ(scene.get_parent_key(child), root);
    EXPECT_EQ(scene.get_parent_key(grandchild), child);

    // Check children relationships
    const auto& root_children = scene.get_children_keys(root);
    ASSERT_EQ(root_children.size(), 1u);
    EXPECT_EQ(root_children[0], child);
    const auto& child_children = scene.get_children_keys(child);
    ASSERT_EQ(child_children.size(), 1u);
    EXPECT_EQ(child_children[0], grandchild);
    const auto& grandchild_children = scene.get_children_keys(grandchild);
    EXPECT_TRUE(grandchild_children.empty());

    // Check name correctness
    const SceneNode* root_node = scene.get_node(root);
    const SceneNode* child_node = scene.get_node(child);
    const SceneNode* grandchild_node = scene.get_node(grandchild);
    ASSERT_NE(root_node, nullptr);
    ASSERT_NE(child_node, nullptr);
    ASSERT_NE(grandchild_node, nullptr);
    EXPECT_EQ(root_node->name, "root");
    EXPECT_EQ(child_node->name, "child");
    EXPECT_EQ(grandchild_node->name, "grandchild");
}

TEST(SceneTest, AddNodeWithInvalidParentUsesRoot) {
    Scene scene{};
    SceneNodeKey invalid_key = SceneNodeKey::null();
    SceneNodeKey node = scene.add_node(invalid_key, "orphan");
    EXPECT_EQ(scene.get_parent_key(node), scene.get_root_key());
    const auto& root_children = scene.get_children_keys(scene.get_root_key());
    EXPECT_EQ(root_children.size(), 1u);
    EXPECT_EQ(root_children[0], node);
    // Check name correctness
    const SceneNode* node_ptr = scene.get_node(node);
    ASSERT_NE(node_ptr, nullptr);
    EXPECT_EQ(node_ptr->name, "orphan");
}

TEST(SceneTest, GetNodeConstReturnsCorrectNode) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    const Scene& const_scene = const_cast<const Scene&>(scene);
    const SceneNode* node_ptr = const_scene.get_node(child);
    ASSERT_NE(node_ptr, nullptr);
    EXPECT_EQ(node_ptr->parent_key, root);
    EXPECT_TRUE(node_ptr->children_keys.empty());
    EXPECT_EQ(node_ptr->name, "child");
}

TEST(SceneTest, GetNodeNonConstReturnsCorrectNode) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    SceneNode* node_ptr = scene.get_node(child);
    ASSERT_NE(node_ptr, nullptr);
    EXPECT_EQ(node_ptr->parent_key, root);
    EXPECT_TRUE(node_ptr->children_keys.empty());
    EXPECT_EQ(node_ptr->name, "child");
}

TEST(SceneTest, MultipleChildren) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child1 = scene.add_node(root, "child1");
    SceneNodeKey child2 = scene.add_node(root, "child2");
    const auto& root_children = scene.get_children_keys(root);
    EXPECT_EQ(root_children.size(), 2u);
    EXPECT_TRUE(
        (root_children[0] == child1 && root_children[1] == child2) ||
        (root_children[0] == child2 && root_children[1] == child1)
    );
    // Check name correctness
    const SceneNode* child1_node = scene.get_node(child1);
    const SceneNode* child2_node = scene.get_node(child2);
    ASSERT_NE(child1_node, nullptr);
    ASSERT_NE(child2_node, nullptr);
    EXPECT_EQ(child1_node->name, "child1");
    EXPECT_EQ(child2_node->name, "child2");
}

TEST(SceneTest, DirtyFlagAndMinimalDirtySetSingleNode) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    SceneNodeKey grandchild = scene.add_node(child, "grandchild");

    // Initially, all nodes except root should be dirty (from add_node)
    EXPECT_TRUE(scene.get_node(child)->dirty);
    EXPECT_TRUE(scene.get_node(grandchild)->dirty);
    EXPECT_FALSE(scene.get_node(root)->dirty);

    // Flush dirty state for further tests
    scene.finalize_and_notify();

    // Set local transform on child, should dirty child and grandchild
    scene.set_local_transform(child, glm::mat4(2.0f));
    EXPECT_TRUE(scene.get_node(child)->dirty);
    EXPECT_TRUE(scene.get_node(grandchild)->dirty);
    EXPECT_FALSE(scene.get_node(root)->dirty);

    // Minimal dirty set should contain only child
    EXPECT_EQ(scene.get_minimal_dirty_set().size(), 1u);
    EXPECT_TRUE(scene.get_minimal_dirty_set().count(child));
    // Node in minimal dirty set should be dirty
    for (auto key : scene.get_minimal_dirty_set()) {
        EXPECT_TRUE(scene.get_node(key)->dirty);
    }
}

TEST(SceneTest, DirtyFlagCoveredByParent) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    SceneNodeKey grandchild = scene.add_node(child, "grandchild");

    // Flush dirty state after node creation
    scene.finalize_and_notify();

    // Set local transform on root, should dirty all
    scene.set_local_transform(root, glm::mat4(2.0f));
    EXPECT_TRUE(scene.get_node(root)->dirty);
    EXPECT_TRUE(scene.get_node(child)->dirty);
    EXPECT_TRUE(scene.get_node(grandchild)->dirty);

    // Minimal dirty set should contain only root
    EXPECT_EQ(scene.get_minimal_dirty_set().size(), 1u);
    EXPECT_TRUE(scene.get_minimal_dirty_set().count(root));
    for (auto key : scene.get_minimal_dirty_set()) {
        EXPECT_TRUE(scene.get_node(key)->dirty);
    }

    // Now set local transform on grandchild, should not add grandchild to minimal dirty set
    scene.set_local_transform(grandchild, glm::mat4(3.0f));
    EXPECT_TRUE(scene.get_node(grandchild)->dirty);
    EXPECT_EQ(scene.get_minimal_dirty_set().size(), 1u);
    EXPECT_TRUE(scene.get_minimal_dirty_set().count(root));
    for (auto key : scene.get_minimal_dirty_set()) {
        EXPECT_TRUE(scene.get_node(key)->dirty);
    }
}

TEST(SceneTest, AddNodeMarksDirtyAndMinimalDirtySet) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    EXPECT_TRUE(scene.get_node(child)->dirty);
    EXPECT_EQ(scene.get_minimal_dirty_set().size(), 1u);
    EXPECT_TRUE(scene.get_minimal_dirty_set().count(child));
    for (auto key : scene.get_minimal_dirty_set()) {
        EXPECT_TRUE(scene.get_node(key)->dirty);
    }
}

TEST(SceneTest, PropagateTransformsTo_RemovesMinimalDirtySetIfSingleChild) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();

    SceneNodeKey child = scene.add_node(root, "child");
    SceneNodeKey grandchild = scene.add_node(child, "grandchild");

    // Flush dirty state after node creation
    scene.finalize_and_notify();

    // Use set_local_transform to dirty only child and grandchild
    scene.set_local_transform(child, glm::mat4(2.0f));

    // Propagate transforms to grandchild
    scene.propagate_transforms_to(grandchild);

    // After propagation, minimal_dirty_set should be empty (child had only one child)
    EXPECT_TRUE(scene.get_minimal_dirty_set().empty());
}

TEST(SceneTest, PropagateTransformsTo_ChainLeavesDescendantsDirty) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    SceneNodeKey grandchild = scene.add_node(child, "grandchild");
    SceneNodeKey greatgrandchild = scene.add_node(grandchild, "greatgrandchild");

    scene.finalize_and_notify();

    // Dirty the whole chain
    scene.set_local_transform(root, glm::mat4(2.0f));

    // Propagate only to grandchild
    scene.propagate_transforms_to(grandchild);

    // grandchild should not be dirty, but greatgrandchild should still be dirty
    EXPECT_FALSE(scene.get_node(grandchild)->dirty);
    EXPECT_TRUE(scene.get_node(greatgrandchild)->dirty);

    // Minimal dirty set should contain greatgrandchild only
    EXPECT_EQ(scene.get_minimal_dirty_set().size(), 1u);
    EXPECT_TRUE(scene.get_minimal_dirty_set().count(greatgrandchild));
    for (auto key : scene.get_minimal_dirty_set()) {
        EXPECT_TRUE(scene.get_node(key)->dirty);
    }

    // changed_nodes should include grandchild
    EXPECT_TRUE(scene.get_changed_nodes().count(grandchild));
}

TEST(SceneTest, PropagateTransformsTo_MultiChildDirtySetBehavior) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child1 = scene.add_node(root, "child1");
    SceneNodeKey child2 = scene.add_node(root, "child2");
    SceneNodeKey child3 = scene.add_node(root, "child3");

    scene.finalize_and_notify();

    // Dirty all children
    scene.set_local_transform(root, glm::mat4(2.0f));

    // Propagate to child2 only
    scene.propagate_transforms_to(child2);

    // child2 should not be dirty, but child1 and child3 should remain dirty
    EXPECT_FALSE(scene.get_node(child2)->dirty);
    EXPECT_TRUE(scene.get_node(child1)->dirty);
    EXPECT_TRUE(scene.get_node(child3)->dirty);

    // Minimal dirty set should contain child1 and child3 only
    EXPECT_EQ(scene.get_minimal_dirty_set().size(), 2u);
    EXPECT_TRUE(scene.get_minimal_dirty_set().count(child1));
    EXPECT_TRUE(scene.get_minimal_dirty_set().count(child3));
    EXPECT_FALSE(scene.get_minimal_dirty_set().count(root));
    for (auto key : scene.get_minimal_dirty_set()) {
        EXPECT_TRUE(scene.get_node(key)->dirty);
    }

    // changed_nodes should include child2
    EXPECT_TRUE(scene.get_changed_nodes().count(child2));
}

TEST(SceneTest, PropagateTransformsTo_ChangedNodesAreCorrect) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    SceneNodeKey grandchild = scene.add_node(child, "grandchild");

    scene.finalize_and_notify();

    // Dirty all
    scene.set_local_transform(root, glm::mat4(2.0f));
    scene.set_local_transform(child, glm::mat4(3.0f));

    // Propagate to grandchild
    scene.propagate_transforms_to(grandchild);

    // changed_nodes should include grandchild, child, and root
    EXPECT_TRUE(scene.get_changed_nodes().count(root));
    EXPECT_TRUE(scene.get_changed_nodes().count(child));
    EXPECT_TRUE(scene.get_changed_nodes().count(grandchild));
}

TEST(SceneTest, PropagateTransforms_ClearsDirtyFlagsAndMinimalDirtySet) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    SceneNodeKey grandchild = scene.add_node(child, "grandchild");

    // Flush dirty state after node creation
    scene.finalize_and_notify();

    // Use set_local_transform to dirty all nodes
    scene.set_local_transform(root, glm::mat4(2.0f));

    scene.propagate_transforms();

    // All nodes should be not dirty
    EXPECT_FALSE(scene.get_node(root)->dirty);
    EXPECT_FALSE(scene.get_node(child)->dirty);
    EXPECT_FALSE(scene.get_node(grandchild)->dirty);
    // Minimal dirty set should be cleared
    EXPECT_TRUE(scene.get_minimal_dirty_set().empty());
}

TEST(SceneTest, FinalizeForRendering_ClearsChangedNodes) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");

    // Flush dirty state after node creation
    scene.finalize_and_notify();

    // Use set_local_transform to dirty child
    scene.set_local_transform(child, glm::mat4(2.0f));

    // After finalize_and_notify, changed_nodes should be cleared
    scene.finalize_and_notify();
    EXPECT_TRUE(scene.get_changed_nodes().empty());
    // Child should be not dirty
    EXPECT_FALSE(scene.get_node(child)->dirty);
    // Minimal dirty set should be cleared
    EXPECT_TRUE(scene.get_minimal_dirty_set().empty());
}

TEST(SceneTest, GetLocalTransformReturnsCorrectValue) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    glm::mat4 custom = glm::mat4(2.0f);
    scene.set_local_transform(root, custom);
    EXPECT_EQ(scene.get_local_transform(root), custom);
}

TEST(SceneTest, GetWorldTransformReturnsCorrectValueAndUpdatesIfDirty) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");

    // Flush dirty state after node creation
    scene.finalize_and_notify();

    glm::mat4 root_local = glm::mat4(2.0f);
    glm::mat4 child_local = glm::mat4(3.0f);
    scene.set_local_transform(root, root_local);
    scene.set_local_transform(child, child_local);

    // Both should be dirty
    EXPECT_TRUE(scene.is_node_dirty(root));
    EXPECT_TRUE(scene.is_node_dirty(child));
    
    // get_world_transform should update and return correct value
    glm::mat4 expected_child_world = root_local * child_local;
    EXPECT_EQ(scene.get_world_transform(child, true), expected_child_world);

    // After call, dirty flags should be cleared
    EXPECT_FALSE(scene.is_node_dirty(root));
    EXPECT_FALSE(scene.is_node_dirty(child));
}

TEST(SceneTest, IsNodeDirtyReturnsCorrectState) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    EXPECT_FALSE(scene.is_node_dirty(root));
    scene.get_node(root)->dirty = true;
    EXPECT_TRUE(scene.is_node_dirty(root));
}

TEST(SceneTest, ObserverReceivesTransformChangeNotification) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    
    // Flush dirty state after node creation
    scene.finalize_and_notify();

    MockSceneObserver observer;
    scene.add_observer(&observer);

    // Use set_local_transform to dirty child
    scene.set_local_transform(child, glm::mat4(2.0f));

    scene.finalize_and_notify();
    EXPECT_EQ(observer.notify_count, 1);
    EXPECT_TRUE(observer.last_changed.count(child));
    EXPECT_FALSE(observer.last_changed.count(root));
}

TEST(SceneTest, ObserverNotNotifiedAfterRemoval) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    
    // Flush dirty state after node creation
    scene.finalize_and_notify();

    MockSceneObserver observer;
    scene.add_observer(&observer);

    // Use set_local_transform to dirty child
    scene.set_local_transform(child, glm::mat4(2.0f));

    scene.remove_observer(&observer);
    scene.finalize_and_notify();
    EXPECT_EQ(observer.notify_count, 0);
}

TEST(SceneTest, MultipleObserversAreNotified) {
    Scene scene{};
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child = scene.add_node(root, "child");
    
    // Flush dirty state after node creation
    scene.finalize_and_notify();
    
    MockSceneObserver observer1, observer2;
    scene.add_observer(&observer1);
    scene.add_observer(&observer2);

    // Use set_local_transform to dirty child
    scene.set_local_transform(child, glm::mat4(2.0f));

    scene.finalize_and_notify();
    EXPECT_EQ(observer1.notify_count, 1);
    EXPECT_EQ(observer2.notify_count, 1);
    EXPECT_TRUE(observer1.last_changed.count(child));
    EXPECT_TRUE(observer2.last_changed.count(child));
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
    SceneNodeKey root = scene.get_root_key();
    SceneNodeKey child1 = scene.add_node(root, "child1");
    SceneNodeKey child2 = scene.add_node(root, "child2");
    SceneNodeKey grandchild1 = scene.add_node(child1, "grandchild1");
    SceneNodeKey grandchild2 = scene.add_node(child1, "grandchild2");
    SceneNodeKey grandchild3 = scene.add_node(child2, "grandchild3");
    
    // Flush dirty state after node creation
    scene.finalize_and_notify();

    MockSceneObserver observer;
    scene.add_observer(&observer);

    // Set local transform on child1, should dirty child1, grandchild1, grandchild2
    scene.set_local_transform(child1, glm::mat4(2.0f));
    EXPECT_TRUE(scene.get_node(child1)->dirty);
    EXPECT_TRUE(scene.get_node(grandchild1)->dirty);
    EXPECT_TRUE(scene.get_node(grandchild2)->dirty);
    EXPECT_FALSE(scene.get_node(child2)->dirty);
    EXPECT_FALSE(scene.get_node(grandchild3)->dirty);

    // Only child1 should be in minimal dirty set
    EXPECT_EQ(scene.get_minimal_dirty_set().size(), 1u);
    EXPECT_TRUE(scene.get_minimal_dirty_set().count(child1));
    for (auto key : scene.get_minimal_dirty_set()) {
        EXPECT_TRUE(scene.get_node(key)->dirty);
    }

    // Finalize and notify, observer should get child1, grandchild1, grandchild2
    scene.finalize_and_notify();
    EXPECT_EQ(observer.notify_count, 1);
    EXPECT_TRUE(observer.last_changed.count(child1));
    EXPECT_TRUE(observer.last_changed.count(grandchild1));
    EXPECT_TRUE(observer.last_changed.count(grandchild2));
    EXPECT_FALSE(observer.last_changed.count(child2));
    EXPECT_FALSE(observer.last_changed.count(grandchild3));

    // Now set local transform on root, which should dirty all nodes
    scene.set_local_transform(root, glm::mat4(3.0f));
    EXPECT_TRUE(scene.get_node(root)->dirty);
    EXPECT_TRUE(scene.get_node(child1)->dirty);
    EXPECT_TRUE(scene.get_node(child2)->dirty);
    EXPECT_TRUE(scene.get_node(grandchild1)->dirty);
    EXPECT_TRUE(scene.get_node(grandchild2)->dirty);
    EXPECT_TRUE(scene.get_node(grandchild3)->dirty);
    EXPECT_EQ(scene.get_minimal_dirty_set().size(), 1u);
    EXPECT_TRUE(scene.get_minimal_dirty_set().count(root));
    for (auto key : scene.get_minimal_dirty_set()) {
        EXPECT_TRUE(scene.get_node(key)->dirty);
    }

    // Finalize and notify, observer should get all nodes
    scene.finalize_and_notify();
    EXPECT_EQ(observer.notify_count, 2);
    EXPECT_TRUE(observer.last_changed.count(root));
    EXPECT_TRUE(observer.last_changed.count(child1));
    EXPECT_TRUE(observer.last_changed.count(child2));
    EXPECT_TRUE(observer.last_changed.count(grandchild1));
    EXPECT_TRUE(observer.last_changed.count(grandchild2));
    EXPECT_TRUE(observer.last_changed.count(grandchild3));

    // Now set local transform on grandchild3 only
    scene.set_local_transform(grandchild3, glm::mat4(4.0f));
    EXPECT_TRUE(scene.get_node(grandchild3)->dirty);
    EXPECT_FALSE(scene.get_node(root)->dirty);
    EXPECT_FALSE(scene.get_node(child1)->dirty);
    EXPECT_FALSE(scene.get_node(child2)->dirty);
    EXPECT_FALSE(scene.get_node(grandchild1)->dirty);
    EXPECT_FALSE(scene.get_node(grandchild2)->dirty);
    EXPECT_EQ(scene.get_minimal_dirty_set().size(), 1u);
    EXPECT_TRUE(scene.get_minimal_dirty_set().count(grandchild3));
    for (auto key : scene.get_minimal_dirty_set()) {
        EXPECT_TRUE(scene.get_node(key)->dirty);
    }

    // Finalize and notify, observer should get only grandchild3
    scene.finalize_and_notify();
    EXPECT_EQ(observer.notify_count, 3);
    EXPECT_TRUE(observer.last_changed.count(grandchild3));
    EXPECT_FALSE(observer.last_changed.count(root));
    EXPECT_FALSE(observer.last_changed.count(child1));
    EXPECT_FALSE(observer.last_changed.count(child2));
    EXPECT_FALSE(observer.last_changed.count(grandchild1));
    EXPECT_FALSE(observer.last_changed.count(grandchild2));
}
