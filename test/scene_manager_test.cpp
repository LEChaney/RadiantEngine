
#include "scene/scene_manager.h"
#include <gtest/gtest.h>

class TestSceneManagerObserver : public ISceneManagerObserver {
public:
    std::vector<std::vector<SceneKey>> notifications;
    void on_active_scenes_changed(const std::vector<SceneKey>& new_active_scenes) override {
        notifications.push_back(new_active_scenes);
    }
};

TEST(SceneManagerTest, AddRemoveScene) {
    SceneManager manager;
    SceneKey s1 = manager.add_scene();
    SceneKey s2 = manager.add_scene();
    EXPECT_NE(manager.get_scene(s1), nullptr);
    EXPECT_NE(manager.get_scene(s2), nullptr);
    manager.remove_scene(s1);
    EXPECT_EQ(manager.get_scene(s1), nullptr);
    EXPECT_NE(manager.get_scene(s2), nullptr);
}

TEST(SceneManagerTest, ActiveSceneSwitching) {
    SceneManager manager;
    SceneKey s1 = manager.add_scene();
    SceneKey s2 = manager.add_scene();
    manager.set_active_scenes({s2});
    const auto& active = manager.get_active_scenes();
    ASSERT_EQ(active.size(), 1);
    EXPECT_EQ(active[0], s2);
    manager.set_active_scenes({s1, s2});
    const auto& active2 = manager.get_active_scenes();
    ASSERT_EQ(active2.size(), 2);
    EXPECT_EQ(active2[0], s1);
    EXPECT_EQ(active2[1], s2);
}

TEST(SceneManagerTest, ObserverNotification) {
    SceneManager manager;
    TestSceneManagerObserver observer;
    manager.add_observer(&observer);
    SceneKey s1 = manager.add_scene();
    SceneKey s2 = manager.add_scene();
    manager.set_active_scenes({s2});
    manager.remove_scene(s2);
    EXPECT_FALSE(observer.notifications.empty());
}
