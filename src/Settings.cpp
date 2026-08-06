#include "Settings.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

AppSettings loadSettings() {
    AppSettings settings;
    std::ifstream cfg("files/settings.cfg");
    if (!cfg) {
        std::cerr << "⚠️  No settings file found, using defaults\n";
        return settings;
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
                        settings.offscreenWidth = w;
                        settings.offscreenHeight = h;
                    }
                } catch (...) {}
            }
        } else if (key == "fps_limit") {
            if (val == "uncapped" || val == "0") settings.targetFPS = 0.0f;
            else try { settings.targetFPS = std::stof(val); } catch (...) {}
        } else if (key == "vsync") {
            settings.vsync = (val == "true" || val == "1");
        }
    }
    std::cout << "🎯 Internal resolution: " << settings.offscreenWidth << "x" << settings.offscreenHeight << "\n";
    std::cout << "⏱️  Target FPS: " << (settings.targetFPS > 0 ? std::to_string((int)settings.targetFPS) : "uncapped") << "\n";
    std::cout << "📺 VSync: " << (settings.vsync ? "ON" : "OFF") << "\n";
    return settings;
}