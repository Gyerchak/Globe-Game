#include "MosaicBuilder.hpp"

#include <gdal_priv.h>
#include <gdal_alg.h>
#include <gdal_utils.h>

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <omp.h>

static std::string tilePath(int latDeg, int lonDeg, const std::string& baseDir) {
    char latDir = (latDeg >= 0) ? 'N' : 'S';
    char lonDir = (lonDeg >= 0) ? 'E' : 'W';
    int absLat = std::abs(latDeg);
    int absLon = std::abs(lonDeg);

    std::ostringstream oss;
    oss << baseDir << "/Copernicus_DSM_10_"
        << latDir << std::setw(2) << std::setfill('0') << absLat << "_00_"
        << lonDir << std::setw(3) << std::setfill('0') << absLon << "_00_DEM";
    return oss.str();
}

static GDALDataset* buildVRT(const std::vector<std::string>& files) {
    if (files.empty()) return nullptr;

    std::vector<const char*> argv;
    argv.push_back("GDALBuildVRT");
    argv.push_back("-overwrite");
    for (const auto& f : files) {
        argv.push_back(f.c_str());
    }

    int argc = static_cast<int>(argv.size());
    GDALBuildVRTOptions* opts = GDALBuildVRTOptionsNew(argv.data(), nullptr);
    GDALDataset* vrtDS = GDALBuildVRT("", opts, nullptr);
    GDALBuildVRTOptionsFree(opts);

    return vrtDS;
}

static bool processChunk(int row, int col, const std::string& dataDir, const std::string& outDir) {
    double lonMin = -180.0 + col * 20.0;
    double lonMax = lonMin + 20.0;
    double latMax = 90.0 - row * 20.0;
    double latMin = latMax - 20.0;

    std::vector<std::string> files;
    for (int lat = static_cast<int>(latMin); lat < static_cast<int>(latMax); ++lat) {
        for (int lon = static_cast<int>(lonMin); lon < static_cast<int>(lonMax); ++lon) {
            std::string path = tilePath(lat, lon, dataDir);
            if (std::filesystem::exists(path + ".tif")) {
                files.push_back(path + ".tif");
            } else if (std::filesystem::exists(path + ".tiff")) {
                files.push_back(path + ".tiff");
            }
        }
    }

    if (files.size() != 400) {
        #pragma omp critical
        std::cerr << "⚠️ Skipping " << row << char('A' + col)
                  << " – found " << files.size() << " tiles (expected 400)." << std::endl;
        return false;
    }

    GDALDataset* vrtDS = buildVRT(files);
    if (!vrtDS) {
        #pragma omp critical
        std::cerr << "❌ Failed to build VRT for " << row << char('A' + col) << std::endl;
        return false;
    }

    std::string outFile = outDir + "/" + std::to_string(row) + char('A' + col) + ".tif";

    std::vector<const char*> warpArgv = {
        "gdalwarp",
        "-of", "GTiff",
        "-co", "COMPRESS=LZW",
        "-co", "PREDICTOR=2",
        "-ts", "1000", "1000",
        "-r", "lanczos",
        "-overwrite"
    };

    std::string vrtName = "/vsimem/temp_" + std::to_string(row) + "_" + std::to_string(col) + ".vrt";
    GDALDataset* vrtMem = GDALCreateCopy("VRT", vrtName.c_str(), vrtDS, false, nullptr, nullptr, nullptr);
    GDALClose(vrtDS);

    if (!vrtMem) {
        #pragma omp critical
        std::cerr << "❌ Failed to copy VRT to memory for " << row << char('A' + col) << std::endl;
        return false;
    }

    warpArgv.push_back(vrtName.c_str());
    warpArgv.push_back(outFile.c_str());

    int argc = static_cast<int>(warpArgv.size());
    GDALWarpOptions* warpOpts = GDALWarpOptionsNew(warpArgv.data(), nullptr);
    GDALDataset* warpedDS = GDALWarp("", nullptr, 1, &vrtMem, warpOpts, nullptr);
    GDALWarpOptionsFree(warpOpts);

    VSIUnlink(vrtName.c_str());
    GDALClose(vrtMem);

    if (!warpedDS) {
        #pragma omp critical
        std::cerr << "❌ Warp failed for " << row << char('A' + col) << std::endl;
        return false;
    }

    GDALClose(warpedDS);

    #pragma omp critical
    std::cout << "✅ Done: " << row << char('A' + col) << std::endl;

    return true;
}

bool buildAllMosaics(const std::string& dataPath, const std::string& outputPath) {
    static bool gdalInit = []() {
        GDALAllRegister();
        return true;
    }();

    if (!std::filesystem::exists(dataPath) || !std::filesystem::is_directory(dataPath)) {
        std::cerr << "❌ Data path does not exist: " << dataPath << std::endl;
        return false;
    }

    std::filesystem::create_directories(outputPath);

    const int rows = 9;
    const int cols = 18;
    int successCount = 0;
    int failCount = 0;

    std::cout << "🚀 Starting mosaic build for " << (rows * cols) << " chunks..." << std::endl;

    #pragma omp parallel for collapse(2) schedule(dynamic) reduction(+:successCount, failCount)
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            if (processChunk(row, col, dataPath, outputPath)) {
                ++successCount;
            } else {
                ++failCount;
            }
        }
    }

    std::cout << "\n🎉 Finished! Success: " << successCount
              << ", Failed: " << failCount << std::endl;

    return (failCount == 0);
}