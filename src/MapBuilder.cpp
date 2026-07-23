#include "MapBuilder.hpp"

#include <iostream>
#include <filesystem>
#include <string>
#include <cstdlib>   // for system()

bool buildAllMosaics(const std::string& dataPath, const std::string& outputPath) {
    std::string vrtPath = dataPath + ".vrt";
    if (!std::filesystem::exists(vrtPath)) {
        std::cerr << "❌ VRT file not found: " << vrtPath << std::endl;
        return false;
    }

    std::filesystem::create_directories(outputPath);

    std::string outFile = outputPath + "/global_mosaic.tif";

    std::cout << "🌍 Generating global mosaic: " << outFile << std::endl;
    std::cout << "📐 Target size: 65536 x 32768 pixels" << std::endl;

    std::string cmd = "gdalwarp -of GTiff -co COMPRESS=LZW -co PREDICTOR=2 "
                      "-ts 65536 32768 -r lanczos -overwrite "
                      "-te -180 -90 180 90 \"" +
                      vrtPath + "\" \"" + outFile + "\"";

    std::cout << "🚀 Running command:\n" << cmd << std::endl;

    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "❌ gdalwarp failed with return code " << ret << std::endl;
        return false;
    }

    std::cout << "✅ Done! Global mosaic created: " << outFile << std::endl;
    return true;
}