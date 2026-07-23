#include "include/MosaicBuilder.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <string>

int main() {
    std::ifstream ifs("config.json");
    if (!ifs.is_open()) {
        std::cerr << "❌ Cannot open config.json" << std::endl;
        return 1;
    }

    nlohmann::json cfg;
    ifs >> cfg;

    std::string dataPath = cfg.value("data_path", "");
    std::string outPath = cfg.value("output_path", "./output/united");

    if (dataPath.empty()) {
        std::cerr << "❌ data_path missing in config.json" << std::endl;
        return 1;
    }

    std::cout << "📂 Data path: " << dataPath << std::endl;
    std::cout << "📂 Output path: " << outPath << std::endl;

    return buildAllMosaics(dataPath, outPath) ? 0 : 1;
}