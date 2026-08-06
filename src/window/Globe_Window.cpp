#include "GlobeApp.h"

void GlobeApp::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "Globe Viewer", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void GlobeApp::framebufferResizeCallback(GLFWwindow* w, int width, int height) {
    auto app = reinterpret_cast<GlobeApp*>(glfwGetWindowUserPointer(w));
    app->framebufferResized = true;
}

void GlobeApp::toggleFullscreen() {
    if (isFullscreen) {
        glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedW, windowedH, 0);
    } else {
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedW, &windowedH);
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    isFullscreen = !isFullscreen;
}

void GlobeApp::keyCallback(GLFWwindow* w, int key, int, int action, int) {
    auto* app = (GlobeApp*)glfwGetWindowUserPointer(w);
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_R: app->camPitch = app->camYaw = app->camRoll = 0.0f; app->camDistance = app->targetDistance = 2.5f; app->camTarget = glm::vec3(0.0f); break;
            case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, GLFW_TRUE); break;
            case GLFW_KEY_F: app->toggleFullscreen(); break;
            case GLFW_KEY_G: app->showGrid = !app->showGrid; break;
            case GLFW_KEY_L: app->showGridLines = !app->showGridLines; break;
            default: break;
        }
        if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
            app->shiftDown = true;
    }
    if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
            app->shiftDown = false;
    }
    if (key >= 0 && key < 512)
        app->keys[key] = (action == GLFW_PRESS || action == GLFW_REPEAT);
}

void GlobeApp::mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    auto* app = (GlobeApp*)glfwGetWindowUserPointer(w);
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        app->rightMouseDown = (action == GLFW_PRESS);
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        app->middleMouseDown = (action == GLFW_PRESS);
    double x, y;
    glfwGetCursorPos(w, &x, &y);
    app->lastMousePos = glm::vec2(x, y);
}

void GlobeApp::cursorPosCallback(GLFWwindow* w, double xpos, double ypos) {
    auto* app = (GlobeApp*)glfwGetWindowUserPointer(w);
    glm::vec2 newPos(xpos, ypos);
    glm::vec2 delta = newPos - app->lastMousePos;
    app->lastMousePos = newPos;
    float orbitSpeed = 0.001f / (app->camDistance * 0.4f + 0.6f);
    if (app->rightMouseDown && !app->shiftDown) {
        app->camYaw += delta.x * orbitSpeed;
        app->camPitch -= delta.y * orbitSpeed;
    }
    if ((app->shiftDown && app->rightMouseDown) || app->middleMouseDown) {
        glm::vec3 camPos = app->getCameraPos();
        glm::vec3 forward = glm::normalize(app->camTarget - camPos);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));
        float scale = app->camDistance * 0.0005f;
        app->camTarget += right * (-delta.x * scale);
        app->camTarget += up * (delta.y * scale);
    }
}

void GlobeApp::scrollCallback(GLFWwindow* w, double, double yoffset) {
    auto* app = (GlobeApp*)glfwGetWindowUserPointer(w);
    float sensitivity = 0.02f;
    float factor = 1.0f - (float)yoffset * sensitivity;
    app->targetDistance *= factor;
    app->targetDistance = glm::clamp(app->targetDistance, 0.8f, 40.0f);
}