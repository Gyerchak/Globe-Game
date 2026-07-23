#include "MapBuilder.hpp"

#include <iostream>
#include <filesystem>
#include <string>
#include <cstdlib>   // for system()
#include <omp.h>     // for omp_get_max_threads

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

    // Detect number of threads (all cores)
    int numThreads = omp_get_max_threads();
    std::cout << "🧵 Using " << numThreads << " threads (OMP_NUM_THREADS=" << numThreads << ")" << std::endl;

    // Build the gdalwarp command with speed flags
    std::string cmd = "gdalwarp -of GTiff -co COMPRESS=LZW -co PREDICTOR=2 "
                      "-co TILED=YES -co BLOCKXSIZE=512 -co BLOCKYSIZE=512 "
                      "-ts 65536 32768 -r lanczos -overwrite "
                      "-wm 4096 -multi "
                      "-wo NUM_THREADS=" + std::to_string(numThreads) + " "
                      "-te -180 -90 180 90 \"" +
                      vrtPath + "\" \"" + outFile + "\"";

    std::cout << "🚀 Running command:\n" << cmd << std::endl;

    // Set environment variable for OpenMP threads (just in case)
    setenv("OMP_NUM_THREADS", std::to_string(numThreads).c_str(), 1);

    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "❌ gdalwarp failed with return code " << ret << std::endl;
        return false;
    }

    std::cout << "✅ Done! Global mosaic created: " << outFile << std::endl;
    return true;
}