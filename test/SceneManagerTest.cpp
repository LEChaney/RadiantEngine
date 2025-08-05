
#include "scene/SceneManager.h"
#include <gtest/gtest.h>

class TestSceneManagerObserver : public ISceneManagerObserver {
public:
    std::vector<std::vector<SceneKey>> notifications;
    void onActiveScenesChanged(const std::vector<SceneKey>& new_active_scenes) override {
        notifications.push_back(new_active_scenes);
    }
};

TEST(SceneManagerTest, AddRemoveScene) {
    SceneManager manager;
    SceneKey s1 = manager.addScene();
    SceneKey s2 = manager.addScene();
    EXPECT_NE(manager.getScene(s1), nullptr);
    EXPECT_NE(manager.getScene(s2), nullptr);
    manager.removeScene(s1);
    EXPECT_EQ(manager.getScene(s1), nullptr);
    EXPECT_NE(manager.getScene(s2), nullptr);
}

TEST(SceneManagerTest, ActiveSceneSwitching) {
    SceneManager manager;
    SceneKey s1 = manager.addScene();
    SceneKey s2 = manager.addScene();
    manager.setActiveScenes({s2});
    const auto& active = manager.getActiveScenes();
    ASSERT_EQ(active.size(), 1);
    EXPECT_EQ(active[0], s2);
    manager.setActiveScenes({s1, s2});
    const auto& active2 = manager.getActiveScenes();
    ASSERT_EQ(active2.size(), 2);
    EXPECT_EQ(active2[0], s1);
    EXPECT_EQ(active2[1], s2);
}

TEST(SceneManagerTest, ObserverNotification) {
    SceneManager manager;
    TestSceneManagerObserver observer;
    manager.addObserver(&observer);
    SceneKey s1 = manager.addScene();
    SceneKey s2 = manager.addScene();
    manager.setActiveScenes({s2});
    manager.removeScene(s2);
    EXPECT_FALSE(observer.notifications.empty());
}
