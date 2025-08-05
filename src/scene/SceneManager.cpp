#include "SceneManager.h"
#include <cassert>

SceneManager::SceneManager() = default;
SceneManager::~SceneManager() = default;

SceneKey SceneManager::addScene() {
    return scenes.add();
}

void SceneManager::removeScene(SceneKey scene) {
    auto it = std::ranges::find(activeScenes, scene);
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
    if (observer && std::ranges::find(observers, observer) == observers.end()) {
        observers.push_back(observer);
    }
}

void SceneManager::removeObserver(ISceneManagerObserver* observer) {
    auto [begin, end] = std::ranges::remove(observers, observer);
    if (begin != end) {
        observers.erase(begin, end);
    }
}
