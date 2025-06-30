# Observer Pattern for System Update Notifications

## Purpose

This document describes the observer pattern as applied to system update notifications within the engine. It defines the rationale, responsibilities, and recommended implementation for notifying dependent systems (such as DrawDataManager, CullingSystem, etc.) of relevant changes in other systems (such as MaterialSystem, MeshSystem, Scene, etc.).

---

## 1. Motivation and Rationale

- Many engine systems depend on timely updates from other systems (e.g., when materials, meshes, or transforms change).
- Direct coupling (manual calls between systems) leads to brittle, hard-to-maintain code and makes it difficult to add new systems or update flows.
- The observer pattern enables decoupled, modular notification of changes, allowing systems to subscribe to relevant events and react accordingly.

---

## 2. Pattern Overview

- **Subject (Observable):** A system that owns data which, when changed, may require other systems to update (e.g., MaterialSystem, MeshSystem, Scene).
- **Observer:** A system that needs to react to changes in another system (e.g., DrawDataManager, CullingSystem).
- **Notification:** When a relevant change occurs, the subject notifies all registered observers, passing sufficient context for the observer to update its state.

---

## 3. Recommended Implementation

- Use explicit observer registration APIs (e.g., `addObserver`, `removeObserver`) on each subject system.
- Observers implement a well-defined interface (e.g., `onMaterialChanged`, `onMeshInstanceAdded`, `onNodeTransformChanged`).
- Subjects call the appropriate observer methods when changes occur.
- Prefer direct function calls or lightweight delegate/event systems for performance-critical notifications; avoid heavy-weight signal/slot or reflection-based systems.
- Observers should be responsible for determining whether the notification is relevant to their state (e.g., filtering by scene or node).
- **Testability and Mocking:**
    - Observer registration should be explicit and performed during system initialization or setup.
    - Systems should accept their subject dependencies (the systems they observe) as constructor or initialization parameters, allowing tests to inject mock or test doubles in place of real systems.
    - This enables unit and integration tests to verify observer behavior in isolation, by substituting mocks that record or assert notification calls.

---

## 4. Example: MaterialSystem Notifying DrawDataManager

```cpp
// In MaterialSystem:
class IMaterialObserver {
public:
    virtual void onMaterialChanged(MaterialHandle handle) = 0;
};

void addObserver(IMaterialObserver* observer);
void removeObserver(IMaterialObserver* observer);

// When a material is updated:
for (auto* observer : observers) {
    observer->onMaterialChanged(changedHandle);
}
```

// In DrawDataManager:
class DrawDataManager : public IMaterialObserver {
    void onMaterialChanged(MaterialHandle handle) override {
        // Update draw data for all draws using this material
    }
};
```

---

## 5. Integration Points

- **MaterialSystem:** Notifies observers (e.g., DrawDataManager) when material data or descriptor sets change.
- **MeshSystem:** Notifies observers when mesh instances are added/removed or per-instance material assignments change.
- **Scene:** Notifies observers when node transforms change (see also the transform propagation and changed nodes set in the Scene module).
- **Other Systems:** Any system with data that may affect rendering or simulation can implement the observer pattern for update notifications.

---

## 6. Best Practices

- Keep observer interfaces minimal and focused on relevant events.
- Avoid deep call chains or circular dependencies between observers and subjects.
- Document all observer interfaces and events in system-level documentation.
- Use the observer pattern only for events that require cross-system updates; avoid overuse for purely internal changes.
- **Testability and Mocking:**
    - Design observer registration so that systems can be easily swapped for mocks or test doubles during testing.
    - Prefer dependency injection (passing subject systems to observers at construction or initialization) over global registration.
    - In tests, provide mock implementations of observer interfaces to verify that notifications are sent and received as expected.
    - Example:
      ```cpp
      // In test setup:
      MockMaterialSystem mockMaterialSystem;
      DrawDataManager drawDataManager;
      mockMaterialSystem.addObserver(&drawDataManager); // Register observer with mock
      // Now, mockMaterialSystem can trigger notifications and drawDataManager's responses can be asserted.
      ```

---

## 7. References

- [Observer Pattern (Wikipedia)](https://en.wikipedia.org/wiki/Observer_pattern)
- [Game Programming Patterns: Observer](https://gameprogrammingpatterns.com/observer.html)
- [Material System Design](../renderer/material_system.md)
- [DrawData Design](../renderer/drawdata.md)
- [Mesh System Design](../renderer/mesh_system.md)
- [Scene Module Design](../scene/scene.md)

---

## 8. Summary Table

| Feature                | Benefit                                      |
|------------------------|----------------------------------------------|
| Explicit registration  | Enables dependency injection and mocking      |
| Mockable observers     | Facilitates unit/integration testing          |

---

This document should be referenced by all systems that use or implement observer-based update notifications. Update as new systems or notification flows are added.
