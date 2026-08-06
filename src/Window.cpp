#include "Window.h"
#include <iostream>

Window::Window(int width, int height, const char* title) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

Window::~Window() {
    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

void Window::pollEvents() {
    input.mouseDelta = glm::vec2(0.0f);
    input.scrollDelta = 0.0f;
    glfwPollEvents();
    input.shouldClose = glfwWindowShouldClose(window);
}

void Window::toggleFullscreen() {
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

void Window::keyCallback(GLFWwindow* w, int key, int, int action, int) {
    auto* app = (Window*)glfwGetWindowUserPointer(w);
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) app->input.shouldClose = true;
        if (key == GLFW_KEY_F) app->toggleFullscreen();
        if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) app->input.shiftDown = true;
    }
    if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) app->input.shiftDown = false;
    }
    if (key >= 0 && key < 512)
        app->input.keys[key] = (action == GLFW_PRESS || action == GLFW_REPEAT);
}

void Window::mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    auto* app = (Window*)glfwGetWindowUserPointer(w);
    if (button == GLFW_MOUSE_BUTTON_RIGHT) app->input.rightMouseDown = (action == GLFW_PRESS);
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) app->input.middleMouseDown = (action == GLFW_PRESS);
}

void Window::cursorPosCallback(GLFWwindow* w, double xpos, double ypos) {
    auto* app = (Window*)glfwGetWindowUserPointer(w);
    static double lastX = xpos, lastY = ypos;
    app->input.mouseDelta = glm::vec2(xpos - lastX, ypos - lastY);
    lastX = xpos; lastY = ypos;
}

void Window::scrollCallback(GLFWwindow* w, double, double yoffset) {
    auto* app = (Window*)glfwGetWindowUserPointer(w);
    app->input.scrollDelta = (float)yoffset;
}

void Window::framebufferResizeCallback(GLFWwindow* w, int, int) {
    auto* app = (Window*)glfwGetWindowUserPointer(w);
    app->input.framebufferResized = true;
}