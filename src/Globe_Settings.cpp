#include "GlobeApp.h"
#include <fstream>
#include <sstream>
#include <algorithm>

void GlobeApp::loadSettings() {
    // Defaults
    offscreenExtent.width = 960;
    offscreenExtent.height = 540;
    targetFPS = 30.0f;
    vsyncEnabled = false;

    std::ifstream cfg("files/settings.cfg");
    if (!cfg) {
        std::cerr << "⚠️  No settings file found, using default 960x540 / 30 FPS / VSync off\n";
        return;
    }

    std::string line;
    while (std::getline(cfg, line)) {
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "resolution") {
            auto xpos = val.find('x');
            if (xpos != std::string::npos) {
                try {
                    uint32_t w = std::stoul(val.substr(0, xpos));
                    uint32_t h = std::stoul(val.substr(xpos + 1));
                    if (w > 0 && h > 0) {
                        offscreenExtent.width = w;
                        offscreenExtent.height = h;
                    }
                } catch (...) {
                    std::cerr << "⚠️  Invalid resolution in settings, keeping default\n";
                }
            }
        }
        else if (key == "fps_limit") {
            if (val == "uncapped" || val == "0") {
                targetFPS = 0.0f;
            } else {
                try {
                    float fps = std::stof(val);
                    if (fps > 0.0f) targetFPS = fps;
                } catch (...) {
                    std::cerr << "⚠️  Invalid fps_limit, keeping default 30\n";
                }
            }
        }
        else if (key == "vsync") {
            if (val == "true" || val == "1") vsyncEnabled = true;
            else if (val == "false" || val == "0") vsyncEnabled = false;
            else std::cerr << "⚠️  Invalid vsync value, keeping default off\n";
        }
    }

    std::cout << "🎯 Internal resolution: " << offscreenExtent.width << "x" << offscreenExtent.height << "\n";
    std::cout << "⏱️  Target FPS: " << (targetFPS > 0.0f ? std::to_string((int)targetFPS) : "uncapped") << "\n";
    std::cout << "📺 VSync: " << (vsyncEnabled ? "ON" : "OFF") << "\n";
}