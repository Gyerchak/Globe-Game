#include "MosaicBuilder.hpp"

#include <gdal_priv.h>
#include <gdal_utils.h>

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <omp.h>

bool buildAllMosaics(const std::string& dataPath, const std::string& outputPath) {
    static bool gdalInit = []() {
        GDALAllRegister();
        return true;
    }();

    // The VRT is in the parent directory of the tiles
    std::string vrtPath = dataPath + ".vrt";
    // If the VRT is elsewhere, you can hardcode it:
    // std::string vrtPath = "/run/media/hubertg/HUB/COP30/COP30_hh.vrt";

    // Open the VRT once for reading
    GDALDataset* srcDS = static_cast<GDALDataset*>(GDALOpen(vrtPath.c_str(), GA_ReadOnly));
    if (!srcDS) {
        std::cerr << "❌ Could not open VRT: " << vrtPath << std::endl;
        return false;
    }
    std::cout << "✅ Opened VRT: " << vrtPath << std::endl;

    std::filesystem::create_directories(outputPath);

    const int rows = 9;
    const int cols = 18;
    int successCount = 0;
    int failCount = 0;

    std::cout << "🚀 Starting mosaic build for " << (rows * cols) << " chunks..." << std::endl;

    // Process chunks in parallel
    #pragma omp parallel for collapse(2) schedule(dynamic) reduction(+:successCount, failCount)
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            // Chunk bounds
            double lonMin = -180.0 + col * 20.0;
            double lonMax = lonMin + 20.0;
            double latMax = 90.0 - row * 20.0;
            double latMin = latMax - 20.0;

            std::string outFile = outputPath + "/" + std::to_string(row) + char('A' + col) + ".tif";

            // Build warp arguments
            std::vector<std::string> warpArgsStr = {
                "gdalwarp",
                "-of", "GTiff",
                "-co", "COMPRESS=LZW",
                "-co", "PREDICTOR=2",
                "-ts", "1000", "1000",
                "-r", "lanczos",
                "-overwrite",
                "-te",
                std::to_string(lonMin),
                std::to_string(latMin),
                std::to_string(lonMax),
                std::to_string(latMax)
            };

            std::vector<const char*> warpArgv;
            warpArgv.reserve(warpArgsStr.size());
            for (const auto& s : warpArgsStr) {
                warpArgv.push_back(s.c_str());
            }
            char** warpArgvPtr = const_cast<char**>(warpArgv.data());

            GDALWarpAppOptions* warpOpts = GDALWarpAppOptionsNew(warpArgvPtr, nullptr);
            if (!warpOpts) {
                #pragma omp critical
                std::cerr << "❌ Failed to create warp options for " << row << char('A' + col) << std::endl;
                failCount++;
                continue;
            }

            // Warp from the VRT
            GDALDatasetH hSrcDS = srcDS;
            GDALDatasetH warpedH = GDALWarp(
                outFile.c_str(),
                nullptr,
                1,
                &hSrcDS,
                warpOpts,
                nullptr
            );
            GDALWarpAppOptionsFree(warpOpts);

            if (!warpedH) {
                #pragma omp critical
                std::cerr << "❌ Warp failed for " << row << char('A' + col) << std::endl;
                failCount++;
                continue;
            }

            GDALClose(warpedH);
            #pragma omp critical
            std::cout << "✅ Done: " << row << char('A' + col) << std::endl;
            successCount++;
        }
    }

    GDALClose(srcDS);

    std::cout << "\n🎉 Finished! Success: " << successCount
              << ", Failed: " << failCount << std::endl;

    return (failCount == 0);
}