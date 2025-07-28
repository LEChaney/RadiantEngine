#include "scene_manager.h"
#include <cassert>

SceneManager::SceneManager() = default;
SceneManager::~SceneManager() = default;

SceneKey SceneManager::addScene() {
    return scenes.add();
}

void SceneManager::removeScene(SceneKey scene) {
    auto it = std::find(activeScenes.begin(), activeScenes.end(), scene);
    if (it != activeScenes.end()) {
        // Remove at swap
        std::iter_swap(it, activeScenes.end() - 1);
        activeScenes.pop_back();
        
        // Notify observers
        for (auto* observer : observers) {
            observer->onActiveScenesChanged(activeScenes);
        }
    }
    scenes.remove(scene);
}

Scene* SceneManager::getScene(SceneKey scene) {
    return scenes.get<Scene>(scene);
}

const Scene* SceneManager::getScene(SceneKey scene) const {
    return scenes.get<Scene>(scene);
}

const std::vector<SceneKey>& SceneManager::getActiveScenes() const {
    return activeScenes;
}

void SceneManager::setActiveScenes(const std::vector<SceneKey>& scenesKeys) {
    activeScenes = scenesKeys;
    // Notify observers
    for (auto* observer : observers) {
        observer->onActiveScenesChanged(activeScenes);
    }
}

void SceneManager::addObserver(ISceneManagerObserver* observer) {
    if (observer && std::find(observers.begin(), observers.end(), observer) == observers.end()) {
        observers.push_back(observer);
    }
}

void SceneManager::removeObserver(ISceneManagerObserver* observer) {
    auto it = std::remove(observers.begin(), observers.end(), observer);
    if (it != observers.end()) {
        observers.erase(it, observers.end());
    }
}
