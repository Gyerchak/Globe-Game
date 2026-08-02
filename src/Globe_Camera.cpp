#include "GlobeApp.h"

glm::vec3 GlobeApp::getCameraPos() {
    float cp = glm::cos(camPitch), sp = glm::sin(camPitch);
    float cy = glm::cos(camYaw), sy = glm::sin(camYaw);
    glm::vec3 pos;
    pos.x = camDistance * cp * sy;
    pos.y = camDistance * sp;
    pos.z = camDistance * cp * cy;
    return pos + camTarget;
}

void GlobeApp::updateCamera() {
    camDistance += (targetDistance - camDistance) * zoomSmoothness;
    camDistance = glm::clamp(camDistance, 0.8f, 500.0f);

    float speed = 0.4f * deltaTime;
    if (keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT]) speed *= 5.0f;
    if (keys[GLFW_KEY_W]) camPitch += speed;
    if (keys[GLFW_KEY_S]) camPitch -= speed;
    if (keys[GLFW_KEY_A]) camYaw -= speed;
    if (keys[GLFW_KEY_D]) camYaw += speed;
    if (keys[GLFW_KEY_Q]) camRoll += speed;
    if (keys[GLFW_KEY_E]) camRoll -= speed;
    if (keys[GLFW_KEY_Z]) { targetDistance *= (1.0f - speed * 0.5f); targetDistance = glm::clamp(targetDistance, 0.8f, 500.0f); }
    if (keys[GLFW_KEY_X]) { targetDistance *= (1.0f + speed * 0.5f); targetDistance = glm::clamp(targetDistance, 0.8f, 500.0f); }

    float cp = glm::cos(camPitch), sp = glm::sin(camPitch);
    float cy = glm::cos(camYaw), sy = glm::sin(camYaw);
    glm::vec3 camPos;
    camPos.x = camDistance * cp * sy;
    camPos.y = camDistance * sp;
    camPos.z = camDistance * cp * cy;
    camPos += camTarget;

    glm::vec3 up(-sp * sy, cp, -sp * cy);
    glm::mat4 view = glm::lookAt(camPos, camTarget, up);
    glm::vec3 forward = glm::normalize(camTarget - camPos);
    view = glm::rotate(view, camRoll, forward);

    float aspect = (float)swapChainExtent.width / (float)swapChainExtent.height;
    float nearPlane = glm::max(0.001f, camDistance * 0.001f);
    float farPlane = glm::max(5000.0f, camDistance * 20.0f);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, nearPlane, farPlane);
    proj[1][1] *= -1;
    proj[2][2] = proj[2][2] * 0.5f + proj[3][2] * 0.5f;
    proj[2][3] = proj[2][3] * 0.5f + proj[3][3] * 0.5f;

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = view;
    ubo.proj = proj;
    ubo.cameraPos = glm::vec4(camPos, 0.0f);
    memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

void GlobeApp::mainLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (!glfwWindowShouldClose(window)) {
        auto cur = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(cur - lastTime).count();
        lastTime = cur;
        glfwPollEvents();
        updateCamera();
        drawFrame();
    }
    vkDeviceWaitIdle(device);
}