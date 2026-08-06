#pragma once
#include <cstdint>

struct AppSettings {
    uint32_t offscreenWidth = 960;
    uint32_t offscreenHeight = 540;
    float targetFPS = 30.0f;          // 0 = uncapped
    bool vsync = false;
    float displacementScale = 0.15f;
};

AppSettings loadSettings();   // <-- add this declaration