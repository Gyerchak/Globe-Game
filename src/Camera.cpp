#include "Camera.h"
#include "Window.h"   // for InputState
#include <algorithm>

void Camera::update(float deltaTime, const InputState& input) {
    camDistance += (targetDistance - camDistance) * zoomSmoothness;
    camDistance = glm::clamp(camDistance, 0.8f, 500.0f);

    float speed = 0.4f * deltaTime;
    if (input.shiftDown) speed *= 5.0f;
    if (input.keys[GLFW_KEY_W]) camPitch += speed;
    if (input.keys[GLFW_KEY_S]) camPitch -= speed;
    if (input.keys[GLFW_KEY_A]) camYaw -= speed;
    if (input.keys[GLFW_KEY_D]) camYaw += speed;
    if (input.keys[GLFW_KEY_Q]) camRoll += speed;
    if (input.keys[GLFW_KEY_E]) camRoll -= speed;
    if (input.keys[GLFW_KEY_Z]) { targetDistance *= (1.0f - speed * 0.5f); targetDistance = glm::clamp(targetDistance, 0.8f, 500.0f); }
    if (input.keys[GLFW_KEY_X]) { targetDistance *= (1.0f + speed * 0.5f); targetDistance = glm::clamp(targetDistance, 0.8f, 500.0f); }

    if (input.scrollDelta != 0.0f) {
        float sensitivity = 0.04f;
        targetDistance *= (1.0f - input.scrollDelta * sensitivity);
        targetDistance = glm::clamp(targetDistance, 0.8f, 500.0f);
    }

    if (input.rightMouseDown && !input.shiftDown) {
        float orbitSpeed = 0.001f / (camDistance * 0.4f + 0.6f);
        camYaw += input.mouseDelta.x * orbitSpeed;
        camPitch -= input.mouseDelta.y * orbitSpeed;
    }
    if ((input.shiftDown && input.rightMouseDown) || input.middleMouseDown) {
        glm::vec3 camPos = viewMatrix()[3]; // not accurate yet, but we'll compute in viewMatrix
        // Quick approximation: use current angles
        float cp = cos(camPitch), sp = sin(camPitch);
        float cy = cos(camYaw), sy = sin(camYaw);
        glm::vec3 forward(cp * sy, sp, cp * cy);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0,1,0)));
        glm::vec3 up = glm::cross(right, forward);
        float scale = camDistance * 0.0005f;
        camTarget += right * (-input.mouseDelta.x * scale);
        camTarget += up * (input.mouseDelta.y * scale);
    }
    if (input.keys[GLFW_KEY_R]) reset();
}

glm::mat4 Camera::viewMatrix() const {
    float cp = cos(camPitch), sp = sin(camPitch);
    float cy = cos(camYaw), sy = sin(camYaw);
    glm::vec3 camPos(camDistance * cp * sy, camDistance * sp, camDistance * cp * cy);
    camPos += camTarget;
    glm::vec3 up(-sp * sy, cp, -sp * cy);
    glm::mat4 view = glm::lookAt(camPos, camTarget, up);
    glm::vec3 forward = glm::normalize(camTarget - camPos);
    view = glm::rotate(view, camRoll, forward);
    return view;
}

glm::mat4 Camera::projectionMatrix(float aspectRatio) const {
    float nearPlane = glm::max(0.001f, camDistance * 0.001f);
    float farPlane = glm::max(5000.0f, camDistance * 20.0f);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspectRatio, nearPlane, farPlane);
    proj[1][1] *= -1;
    proj[2][2] = proj[2][2] * 0.5f + proj[3][2] * 0.5f;
    proj[2][3] = proj[2][3] * 0.5f + proj[3][3] * 0.5f;
    return proj;
}

void Camera::reset() {
    camPitch = camYaw = camRoll = 0.0f;
    camDistance = targetDistance = 2.5f;
    camTarget = glm::vec3(0.0f);
}