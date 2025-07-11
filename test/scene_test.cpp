#include "scene/scene.h"
#include <gtest/gtest.h>
#include <string>

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
