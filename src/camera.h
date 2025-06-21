#include "vk_types.h"
#include "SDL_events.h"
#include <array>
#include <vulkan/vulkan_core.h>

struct FrustumPlane {
    glm::vec3 normal;
    float d;
};

class Camera {
public:
    glm::vec3 velocity;
    glm::vec3 position;
    float pitch { 0.f };
    float yaw { 0.f };
    float movementSpeed { 0.25f }; // Camera movement speed

    glm::mat4 getViewMatrix();
    glm::mat4 getRotationMatrix();
    void processSDLEvent(SDL_Event& e);
    void update();

    // Encapsulated projection/frustum update
    void updateProjectionAndFrustum(VkExtent2D extent, float fov, float near, float far);
    const glm::mat4& getProjection() const { return _cachedProj; }
    const std::array<FrustumPlane, 6>& getFrustumPlanesVS() const { return _cachedFrustumPlanes; }

private:
    VkExtent2D _lastExtent {0, 0};
    glm::mat4 _cachedProj {1.0f};
    std::array<FrustumPlane, 6> _cachedFrustumPlanes;
};
