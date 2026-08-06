#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <functional>

struct InputState {
    bool rightMouseDown = false;
    bool middleMouseDown = false;
    bool shiftDown = false;
    glm::vec2 mouseDelta;
    float scrollDelta = 0.0f;
    bool keys[512] = {};
    bool framebufferResized = false;
    bool shouldClose = false;
};

class Window {
public:
    Window(int width = 1280, int height = 720, const char* title = "Globe Viewer");
    ~Window();
    GLFWwindow* handle() const { return window; }
    void pollEvents();
    bool isKeyPressed(int key) const { return input.keys[key]; }
    InputState& getInput() { return input; }
    void toggleFullscreen();

private:
    GLFWwindow* window = nullptr;
    InputState input;
    bool isFullscreen = false;
    GLFWmonitor* monitor = nullptr;
    int windowedX = 0, windowedY = 0, windowedW = 1280, windowedH = 720;

    static void keyCallback(GLFWwindow* w, int key, int, int action, int);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int);
    static void cursorPosCallback(GLFWwindow* w, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* w, double xoffset, double yoffset);
    static void framebufferResizeCallback(GLFWwindow* w, int width, int height);
};