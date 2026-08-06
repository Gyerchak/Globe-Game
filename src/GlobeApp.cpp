#include "GlobeApp.h"
#include "Settings.h"          // for loadSettings()
#include <chrono>
#include <thread>
#include <iostream>            // <-- add this

void GlobeApp::run() {
    settings = loadSettings();          // now declared in Settings.h
    renderer = std::make_unique<Renderer>(&window, settings);
    renderer->initResources();          // load sphere, textures, grid...

    auto lastTime = std::chrono::high_resolution_clock::now();
    const double targetFrameDuration = (settings.targetFPS > 0) ? (1.0 / settings.targetFPS) : 0.0;

    while (!window.getInput().shouldClose) {
        auto frameStart = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(frameStart - lastTime).count();
        lastTime = frameStart;

        window.pollEvents();

        auto& input = window.getInput();
        // Example key toggles (you can move them into Camera or dedicated input handling)
        static bool gPressed = false, lPressed = false;
        if (input.keys[GLFW_KEY_G] && !gPressed) { showGrid = !showGrid; gPressed = true; }
        if (!input.keys[GLFW_KEY_G]) gPressed = false;
        if (input.keys[GLFW_KEY_L] && !lPressed) { showGridLines = !showGridLines; lPressed = true; }
        if (!input.keys[GLFW_KEY_L]) lPressed = false;

        camera.update(deltaTime, input);
        renderer->drawFrame(camera, showGrid, showGridLines, settings.displacementScale);

        if (settings.targetFPS > 0) {
            auto frameEnd = std::chrono::high_resolution_clock::now();
            double workTime = std::chrono::duration<double>(frameEnd - frameStart).count();
            if (workTime < targetFrameDuration)
                std::this_thread::sleep_for(std::chrono::duration<double>(targetFrameDuration - workTime));
        }
    }
    renderer->waitIdle();
}

int main() {
    try {
        GlobeApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}