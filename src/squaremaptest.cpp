// squaremaptest.cpp
// Resize the 2:1 colour map to a square (height stretched to match width).
// Compile: g++ -std=c++20 -O3 squaremaptest.cpp -o exe/squaremaptest -lgdal
// Run: ./exe/squaremaptest

#include <gdal_priv.h>
#include <gdal_utils.h>
#include <cpl_conv.h>

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace fs = std::filesystem;

int main() {
    GDALAllRegister();
    std::ios::sync_with_stdio(false);

    // Input: from output/ (where CompressHeightMap placed it)
    fs::path inPath = fs::current_path() / "output" / "heightmap_color.tif";   // <-- underscore
    fs::path outPath = fs::current_path() / "output" / "squarecolormap.tif";
    fs::path inCopyPath = fs::current_path() / "input" / "squarecolormap.tif";

    if (!fs::exists(inPath)) {
        std::cerr << "❌ Input file not found: " << inPath << "\n";
        std::cerr << "   Please ensure you have run CompressHeightMap first.\n";
        return 1;
    }
    fs::create_directories(outPath.parent_path());
    fs::create_directories(inCopyPath.parent_path());

    GDALDataset* src = (GDALDataset*)GDALOpen(inPath.c_str(), GA_ReadOnly);
    if (!src) {
        std::cerr << "❌ Could not open " << inPath << "\n";
        return 1;
    }

    const int targetSize = 16384; // 16K – adjust if you want larger/smaller
    std::cout << "📐 Resizing from " << src->GetRasterXSize() << "x" << src->GetRasterYSize()
    << " to " << targetSize << " x " << targetSize << "\n";

    std::vector<std::string> warpArgsStr = {
        "-of", "GTiff",
        "-ts", std::to_string(targetSize), std::to_string(targetSize),
        "-r", "lanczos",
        "-overwrite"
    };
    std::vector<const char*> warpArgv;
    warpArgv.reserve(warpArgsStr.size());
    for (auto& s : warpArgsStr) warpArgv.push_back(s.c_str());
    char** warpArgvPtr = const_cast<char**>(warpArgv.data());

    GDALWarpAppOptions* warpOpts = GDALWarpAppOptionsNew(warpArgvPtr, nullptr);
    if (!warpOpts) {
        std::cerr << "❌ Failed to create warp options\n";
        GDALClose(src);
        return 1;
    }

    GDALDataset* dst = (GDALDataset*)GDALWarp(outPath.c_str(), nullptr, 1,
                                              (GDALDatasetH*)&src, warpOpts, nullptr);
    GDALWarpAppOptionsFree(warpOpts);
    GDALClose(src);

    if (!dst) {
        std::cerr << "❌ Warp failed\n";
        return 1;
    }

    GDALClose(dst);
    std::cout << "✅ Square map written to " << outPath << "\n";

    fs::copy_file(outPath, inCopyPath, fs::copy_options::overwrite_existing);
    std::cout << "📁 Copied to " << inCopyPath << "\n";

    return 0;
}
