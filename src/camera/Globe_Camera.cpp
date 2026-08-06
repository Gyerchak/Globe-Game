#include "GlobeApp.h"
#include <thread>

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
    camDistance = glm::clamp(camDistance, 0.8f, 1200.0f);

    float speed = 0.4f * deltaTime;
    if (keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT]) speed *= 5.0f;
    bool ctrlDown = keys[GLFW_KEY_LEFT_CONTROL] || keys[GLFW_KEY_RIGHT_CONTROL];

    // When Ctrl is held, interpret WASD as camera-target panning (move the "head" in X/Y)
    if (ctrlDown) {
        glm::vec3 camPos = getCameraPos();
        glm::vec3 forward = glm::normalize(camTarget - camPos);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));
        float panSpeed = glm::clamp(camDistance * 0.0025f, 0.01f, 10.0f) * (shiftDown ? 5.0f : 1.0f);
        if (keys[GLFW_KEY_W]) camTarget += up * panSpeed;
        if (keys[GLFW_KEY_S]) camTarget -= up * panSpeed;
        if (keys[GLFW_KEY_A]) camTarget -= right * panSpeed;
        if (keys[GLFW_KEY_D]) camTarget += right * panSpeed;
        // allow altitude changes with Q/E while panning
        if (keys[GLFW_KEY_Q]) camTarget += forward * panSpeed * 0.5f;
        if (keys[GLFW_KEY_E]) camTarget -= forward * panSpeed * 0.5f;
    } else {
        if (keys[GLFW_KEY_W]) camPitch += speed;
        if (keys[GLFW_KEY_S]) camPitch -= speed;
        if (keys[GLFW_KEY_A]) camYaw -= speed;
        if (keys[GLFW_KEY_D]) camYaw += speed;
        if (keys[GLFW_KEY_Q]) camRoll += speed;
        if (keys[GLFW_KEY_E]) camRoll -= speed;
    }
    if (keys[GLFW_KEY_Z]) { targetDistance -= speed * 1.5f; targetDistance = glm::clamp(targetDistance, 0.8f, 1200.0f); }
    if (keys[GLFW_KEY_X]) { targetDistance += speed * 1.5f; targetDistance = glm::clamp(targetDistance, 0.8f, 1200.0f); }

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
    float nearPlane = 0.01f;
    float farPlane = glm::max(5000.0f, camDistance * 50.0f);

    // Distance-dependent field-of-view to make zooming out reveal more of the globe
    // rather than simply shrinking it to a tiny circle.
    const float baseFovDeg = 40.0f;      // FOV at close range
    const float maxFovDeg = 175.0f;     // allow an even wider FOV when zoomed out
    const float fovStartDist = 0.9f;    // start opening the FOV very early
    const float fovEndDist = 4.0f;      // reach max FOV at a modest distance
    float t = 0.0f;
    if (camDistance > fovStartDist) {
        // faster opening curve so small distance changes expose much more area
        t = (camDistance - fovStartDist) / (fovEndDist - fovStartDist);
        t = glm::clamp(t, 0.0f, 1.0f);
        // ease out to emphasize opening early
        t = glm::pow(t, 0.5f);
    }
    float fovDeg = glm::mix(baseFovDeg, maxFovDeg, t);
    glm::mat4 proj = glm::perspective(glm::radians(fovDeg), aspect, nearPlane, farPlane);

    // (Debug logging removed)
    proj[1][1] *= -1;
    proj[2][2] = proj[2][2] * 0.5f + proj[3][2] * 0.5f;
    proj[2][3] = proj[2][3] * 0.5f + proj[3][3] * 0.5f;

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = view;
    ubo.proj = proj;
    ubo.cameraPos = glm::vec4(camPos, 0.0f);
    ubo.displacementScale = 0.03f;   // Heightmap strength – adjust as needed
    memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

void GlobeApp::mainLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    const double targetFrameDuration = (targetFPS > 0.0f) ? (1.0 / targetFPS) : 0.0;

    while (!glfwWindowShouldClose(window)) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        auto cur = frameStart;
        deltaTime = std::chrono::duration<float>(cur - lastTime).count();
        lastTime = cur;

        glfwPollEvents();

        if (framebufferResized) {
            recreateSwapChain();
        }

        updateCamera();
        drawFrame();

        if (targetFPS > 0.0f) {
            auto frameEnd = std::chrono::high_resolution_clock::now();
            double workTime = std::chrono::duration<double>(frameEnd - frameStart).count();
            if (workTime < targetFrameDuration) {
                double sleepTime = targetFrameDuration - workTime;
                std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
            }
        }
    }
    vkDeviceWaitIdle(device);
}