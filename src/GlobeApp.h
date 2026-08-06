#pragma once
#include "Settings.h"
#include "Window.h"
#include "Camera.h"
#include "Renderer.h"
#include <memory>                    // <-- add this

class GlobeApp {
public:
    void run();
private:
    AppSettings settings;
    Window window{1280, 720, "Globe Viewer"};
    Camera camera;
    std::unique_ptr<Renderer> renderer;
    bool showGrid = false, showGridLines = false;
};