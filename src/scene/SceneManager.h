#pragma once

#include "scene/Scene.h"
#include <vector>
#include <algorithm>

class ISceneManagerObserver {
public:
    virtual ~ISceneManagerObserver() = default;
    virtual void onActiveScenesChanged(
        const std::vector<SceneKey>& newActiveScenes
    ) = 0;
};

class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    SceneKey addScene();
    void removeScene(SceneKey scene);
    Scene* getScene(SceneKey scene);
    const Scene* getScene(SceneKey scene) const;
    const std::vector<SceneKey>& getActiveScenes() const;
    void setActiveScenes(const std::vector<SceneKey>& scenes);
    void addObserver(ISceneManagerObserver* observer);
    void removeObserver(ISceneManagerObserver* observer);

private:
    SlotMap<Scene> scenes;
    std::vector<SceneKey> activeScenes;
    std::vector<ISceneManagerObserver*> observers;
};
