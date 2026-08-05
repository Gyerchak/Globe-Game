#include "GlobeApp.h"
#include <fstream>
#include <sstream>
#include <algorithm>

void GlobeApp::loadSettings() {
    // Default fallback
    offscreenExtent.width = 960;
    offscreenExtent.height = 540;

    std::ifstream cfg("file/settings.cfg");
    if (!cfg) {
        std::cerr << "⚠️  No settings file found, using default 960x540\n";
        return;
    }

    std::string line;
    while (std::getline(cfg, line)) {
        // Remove whitespace
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
    }

    std::cout << "🎯 Internal render resolution: "
              << offscreenExtent.width << "x" << offscreenExtent.height << "\n";
}