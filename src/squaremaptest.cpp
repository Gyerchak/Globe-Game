// squaremaptest.cpp
// Resize the 2:1 colour map to a square using gdalwarp (subprocess).
// Compile: g++ -std=c++20 -O3 squaremaptest.cpp -o exe/squaremaptest
// Run: ./exe/squaremaptest [targetSize]   (default: 16384)

#include <iostream>
#include <filesystem>
#include <string>
#include <cstdlib>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    int targetSize = 16384;
    if (argc >= 2) {
        targetSize = std::stoi(argv[1]);
    }

    std::cout << "📐 Target size: " << targetSize << " x " << targetSize << "\n";
    if (targetSize > 32768) {
        std::cerr << "⚠️  WARNING: Target size > 32768 will produce a huge file (> 3 GB) and cannot be loaded by most GPUs.\n";
        std::cerr << "   The output will be generated but the viewer will fail to load it.\n";
        std::cerr << "   Consider using 16384 or 32768 for real‑time viewing.\n";
    }

    fs::path inPath = fs::current_path() / "output" / "heightmap_color.tif";
    fs::path outPath = fs::current_path() / "output" / "squarecolormap.tif";
    fs::path inCopyPath = fs::current_path() / "input" / "squarecolormap.tif";

    if (!fs::exists(inPath)) {
        std::cerr << "❌ Input file not found: " << inPath << "\n";
        std::cerr << "   Please run CompressHeightMap first.\n";
        return 1;
    }

    fs::create_directories(outPath.parent_path());
    fs::create_directories(inCopyPath.parent_path());

    // Use a fixed cache of 8 GB – safe and accepted by GDAL
    const int cacheMB = 8192;

    // Build the gdalwarp command (no -progress)
    std::string cmd = "gdalwarp -of GTiff "
    "-co COMPRESS=LZW "
    "-co PREDICTOR=2 "
    "-co TILED=YES "
    "-co BLOCKXSIZE=512 "
    "-co BLOCKYSIZE=512 "
    "-wm " + std::to_string(cacheMB) + " "
    "-multi "
    "-wo NUM_THREADS=ALL_CPUS "
    "-ts " + std::to_string(targetSize) + " " + std::to_string(targetSize) + " "
    "-r lanczos "
    "-overwrite "
    "\"" + inPath.string() + "\" "
    "\"" + outPath.string() + "\"";

    std::cout << "🚀 Running:\n" << cmd << "\n";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "❌ gdalwarp failed with return code " << ret << "\n";
        return 1;
    }

    std::cout << "✅ Square map written to " << outPath << "\n";

    // If size is ≤ 32768, copy to input/ for viewer
    if (targetSize <= 32768) {
        fs::copy_file(outPath, inCopyPath, fs::copy_options::overwrite_existing);
        std::cout << "📁 Copied to " << inCopyPath << " (for viewer)\n";
    } else {
        std::cerr << "⚠️  Output is too large for the viewer; it will NOT be copied to input/.\n";
        std::cerr << "   You can use this file for offline processing only.\n";
        std::cout << "   For viewing, generate a smaller version: ./exe/squaremaptest 16384\n";
    }

    return 0;
}
