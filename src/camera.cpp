#include <camera.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <set>

// Helper: Extracts the 6 frustum planes from a projection matrix (view space)
static std::array<FrustumPlane, 6> extract_frustum_planes_proj(const glm::mat4& proj) {
    std::array<FrustumPlane, 6> planes;
    // Left
    planes[0].normal.x = proj[0][3] + proj[0][0];
    planes[0].normal.y = proj[1][3] + proj[1][0];
    planes[0].normal.z = proj[2][3] + proj[2][0];
    planes[0].d        = proj[3][3] + proj[3][0];
    // Right
    planes[1].normal.x = proj[0][3] - proj[0][0];
    planes[1].normal.y = proj[1][3] - proj[1][0];
    planes[1].normal.z = proj[2][3] - proj[2][0];
    planes[1].d        = proj[3][3] - proj[3][0];
    // Bottom
    planes[2].normal.x = proj[0][3] + proj[0][1];
    planes[2].normal.y = proj[1][3] + proj[1][1];
    planes[2].normal.z = proj[2][3] + proj[2][1];
    planes[2].d        = proj[3][3] + proj[3][1];
    // Top
    planes[3].normal.x = proj[0][3] - proj[0][1];
    planes[3].normal.y = proj[1][3] - proj[1][1];
    planes[3].normal.z = proj[2][3] - proj[2][1];
    planes[3].d        = proj[3][3] - proj[3][1];
    // Near
    planes[4].normal.x = proj[0][3] + proj[0][2];
    planes[4].normal.y = proj[1][3] + proj[1][2];
    planes[4].normal.z = proj[2][3] + proj[2][2];
    planes[4].d        = proj[3][3] + proj[3][2];
    // Far
    planes[5].normal.x = proj[0][3] - proj[0][2];
    planes[5].normal.y = proj[1][3] - proj[1][2];
    planes[5].normal.z = proj[2][3] - proj[2][2];
    planes[5].d        = proj[3][3] - proj[3][2];
    // Normalize
    for (auto& plane : planes) {
        float len = glm::length(plane.normal);
        if (len > 0.0f) {
            plane.normal /= len;
            plane.d /= len;
        }
    }
    return planes;
}

void Camera::updateProjectionAndFrustum(VkExtent2D extent, float fov, float near, float far) {
    if (extent.width != _lastExtent.width || extent.height != _lastExtent.height) {
        float aspect = (float)extent.width / (float)extent.height;
        _cachedProj = glm::perspective(glm::radians(fov), aspect, near, far);
        _cachedProj[1][1] *= -1.0f;
        _cachedFrustumPlanes = extract_frustum_planes_proj(_cachedProj);
        _lastExtent = extent;
    }
}

void Camera::update()
{
    glm::mat4 cameraRotation = getRotationMatrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * movementSpeed, 0.f));
}

void Camera::processSDLEvent(SDL_Event& e)
{
    static bool rightMouseButtonDown = false;
    static std::set<SDL_Keycode> keysPressed;

    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
        rightMouseButtonDown = true;
        SDL_SetRelativeMouseMode(SDL_TRUE); // Lock the mouse pointer to the window
    }

    if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) {
        rightMouseButtonDown = false;
        SDL_SetRelativeMouseMode(SDL_FALSE); // Unlock the mouse pointer
    }

    velocity = glm::vec3(0.f); // Reset velocity

    if (e.type == SDL_KEYDOWN) {
        keysPressed.insert(e.key.keysym.sym);
    }

    if (e.type == SDL_KEYUP) {
        keysPressed.erase(e.key.keysym.sym);
    }

    if (rightMouseButtonDown) {
        // Adjust camera speed with scroll wheel
        if (e.type == SDL_MOUSEWHEEL) {
            if (e.wheel.y > 0) {
                movementSpeed *= 1.1f; // Increase speed
            } else if (e.wheel.y < 0) {
                movementSpeed *= 0.9f; // Decrease speed
            }
            if (movementSpeed < 0.01f) movementSpeed = 0.01f; // Clamp to minimum
            if (movementSpeed > 100.0f) movementSpeed = 100.0f; // Clamp to maximum
        }

        if (keysPressed.count(SDLK_w)) { velocity.z -= 1; }
        if (keysPressed.count(SDLK_s)) { velocity.z += 1; }
        if (keysPressed.count(SDLK_a)) { velocity.x -= 1; }
        if (keysPressed.count(SDLK_d)) { velocity.x += 1; }
        if (keysPressed.count(SDLK_q)) { velocity.y -= 1; }
        if (keysPressed.count(SDLK_e)) { velocity.y += 1; }

        if (e.type == SDL_MOUSEMOTION) {
            yaw += (float)e.motion.xrel / 200.f;
            pitch -= (float)e.motion.yrel / 200.f;
        }
    }
}

glm::mat4 Camera::getViewMatrix()
{
    // to create a correct model view, we need to move the world in opposite
    // direction to the camera
    //  so we will create the camera model matrix and invert
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    glm::mat4 cameraRotation = getRotationMatrix();
    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix()
{
    // fairly typical FPS style camera. we join the pitch and yaw rotations into
    // the final rotation matrix

    glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
    glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}