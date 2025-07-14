#include "scene_manager.h"
#include <cassert>

SceneManager::SceneManager() = default;
SceneManager::~SceneManager() = default;

SceneKey SceneManager::add_scene() {
    return scenes.add();
}

void SceneManager::remove_scene(SceneKey scene) {
    auto it = std::find(active_scenes.begin(), active_scenes.end(), scene);
    if (it != active_scenes.end()) {
        // Remove at swap
        std::iter_swap(it, active_scenes.end() - 1);
        active_scenes.pop_back();
        
        // Notify observers
        for (auto* observer : observers) {
            observer->on_active_scenes_changed(active_scenes);
        }
    }
    scenes.remove(scene);
}

Scene* SceneManager::get_scene(SceneKey scene) {
    return scenes.get<Scene>(scene);
}

const Scene* SceneManager::get_scene(SceneKey scene) const {
    return scenes.get<Scene>(scene);
}

const std::vector<SceneKey>& SceneManager::get_active_scenes() const {
    return active_scenes;
}

void SceneManager::set_active_scenes(const std::vector<SceneKey>& scenes_keys) {
    active_scenes = scenes_keys;
    // Notify observers
    for (auto* observer : observers) {
        observer->on_active_scenes_changed(active_scenes);
    }
}

void SceneManager::add_observer(ISceneManagerObserver* observer) {
    if (observer && std::find(observers.begin(), observers.end(), observer) == observers.end()) {
        observers.push_back(observer);
    }
}

void SceneManager::remove_observer(ISceneManagerObserver* observer) {
    auto it = std::remove(observers.begin(), observers.end(), observer);
    if (it != observers.end()) {
        observers.erase(it, observers.end());
    }
}
