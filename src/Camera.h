#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct InputState;

class Camera {
public:
    void update(float deltaTime, const InputState& input);
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspectRatio) const;
    void reset();

private:
    float camDistance = 2.5f, targetDistance = 2.5f;
    const float zoomSmoothness = 0.18f;
    float camPitch = 0.0f, camYaw = 0.0f, camRoll = 0.0f;
    glm::vec3 camTarget = glm::vec3(0.0f);
};