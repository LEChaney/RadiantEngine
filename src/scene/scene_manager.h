#pragma once

#include "scene.h"
#include <vector>
#include <algorithm>

class ISceneManagerObserver {
public:
    virtual ~ISceneManagerObserver() = default;
    virtual void on_active_scenes_changed(
        const std::vector<SceneKey>& new_active_scenes
    ) = 0;
};

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    SceneKey add_scene();
    void remove_scene(SceneKey scene);
    Scene* get_scene(SceneKey scene);
    const Scene* get_scene(SceneKey scene) const;
    const std::vector<SceneKey>& get_active_scenes() const;
    void set_active_scenes(const std::vector<SceneKey>& scenes);
    void add_observer(ISceneManagerObserver* observer);
    void remove_observer(ISceneManagerObserver* observer);

private:
    SlotMap<Scene> scenes;
    std::vector<SceneKey> active_scenes;
    std::vector<ISceneManagerObserver*> observers;
};
