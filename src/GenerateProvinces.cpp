// GenerateProvinces.cpp
// Compile: g++ -std=c++20 -O3 GenerateProvinces.cpp -o exe/GenerateProvinces -lgdal
// Run: ./exe/GenerateProvinces

#include <gdal_priv.h>
#include <gdal_utils.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <random>
#include <string>
#include <cstdint>

namespace fs = std::filesystem;

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 32768;

// Hexagon parameters (pointy‑top)
constexpr double HEX_RADIUS = 7.0;          // pixels (centre to corner)
constexpr double DX = std::sqrt(3.0) * HEX_RADIUS;   // horizontal spacing
constexpr double DY = 1.5 * HEX_RADIUS;              // vertical spacing

// -----------------------------------------------------------------------------
// Hex geometry (pointy‑top)
// -----------------------------------------------------------------------------
struct Hex { int q, r; };

static inline std::pair<double,double> hexToPixel(int q, int r, double xOff, double yOff) {
    double x = xOff + HEX_RADIUS * std::sqrt(3.0) * (q + r * 0.5);
    double y = yOff + HEX_RADIUS * 1.5 * r;
    return {x, y};
}

static inline Hex pixelToHex(double x, double y, double xOff, double yOff) {
    double q = (x - xOff) / (std::sqrt(3.0) * HEX_RADIUS) - (y - yOff) / (1.5 * HEX_RADIUS) * 0.5;
    double r = (y - yOff) / (1.5 * HEX_RADIUS);
    double s = -q - r;
    int qi = static_cast<int>(std::round(q));
    int ri = static_cast<int>(std::round(r));
    int si = static_cast<int>(std::round(s));
    double qdiff = std::abs(q - qi);
    double rdiff = std::abs(r - ri);
    double sdiff = std::abs(s - si);
    if (qdiff > rdiff && qdiff > sdiff) {
        qi = -ri - si;
    } else if (rdiff > sdiff) {
        ri = -qi - si;
    } else {
        si = -qi - ri;
    }
    return {qi, ri};
}

// -----------------------------------------------------------------------------
// Deterministic colour from province ID (hash)
// -----------------------------------------------------------------------------
static inline std::array<uint8_t, 3> colorFromID(uint32_t id) {
    uint32_t hash = id * 2654435761u;  // golden ratio constant
    uint32_t rgb = hash & 0xFFFFFF;    // 24-bit colour
    return {
        static_cast<uint8_t>((rgb >> 16) & 0xFF),
        static_cast<uint8_t>((rgb >> 8) & 0xFF),
        static_cast<uint8_t>(rgb & 0xFF)
    };
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    GDALAllRegister();

    fs::path inHeight = fs::current_path() / "input" / "heightmap_8bit.tif";
    fs::path outBin = fs::current_path() / "output" / "province.bin";
    fs::path outTxt = fs::current_path() / "output" / "provinces.txt";
    fs::path outTif = fs::current_path() / "output" / "provincemap.tif";

    if (!fs::exists(inHeight)) {
        std::cerr << "❌ Input heightmap not found: " << inHeight << "\n";
        return 1;
    }
    fs::create_directories(outBin.parent_path());

    GDALDataset* srcDS = (GDALDataset*)GDALOpen(inHeight.c_str(), GA_ReadOnly);
    if (!srcDS) {
        std::cerr << "❌ Could not open " << inHeight << "\n";
        return 1;
    }

    int width = srcDS->GetRasterXSize();
    int height = srcDS->GetRasterYSize();
    GDALRasterBand* band = srcDS->GetRasterBand(1);

    // ---------- Hex grid ----------
    double xOff = 0.0, yOff = 0.0;
    int cols = static_cast<int>(std::ceil(width / DX)) + 2;
    int rows = static_cast<int>(std::ceil(height / DY)) + 2;

    std::cout << "🗺️  Hex grid: " << cols << " x " << rows << " = "
              << (cols * rows) << " cells\n";

    // ---------- First pass: mark land cells ----------
    std::cout << "🔎 First pass: scanning for land cells...\n";
    std::vector<uint8_t> rowIn(width);
    std::vector<std::vector<bool>> cellHasLand(rows, std::vector<bool>(cols, false));

    for (int y = 0; y < height; ++y) {
        band->RasterIO(GF_Read, 0, y, width, 1, rowIn.data(), width, 1, GDT_Byte, 0, 0);
        for (int x = 0; x < width; ++x) {
            if (rowIn[x] != 0) {
                Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
                int col = h.q + (h.r - (h.r & 1)) / 2;
                int row = h.r;
                if (col < 0) col = 0;
                if (col >= cols) col = cols - 1;
                if (row < 0) row = 0;
                if (row >= rows) row = rows - 1;
                cellHasLand[row][col] = true;
            }
        }
        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    // ---------- Collect land cells ----------
    std::vector<std::pair<int, int>> landCells;
    landCells.reserve(rows * cols / 2);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellHasLand[r][c]) landCells.push_back({r, c});
        }
    }

    uint32_t numProvinces = static_cast<uint32_t>(landCells.size());
    std::cout << "🏛️  Number of provinces (land hexagons): " << numProvinces << "\n";

    // Assign IDs (1‑based)
    std::vector<std::vector<uint32_t>> cellId(rows, std::vector<uint32_t>(cols, 0));
    for (uint32_t idx = 0; idx < numProvinces; ++idx) {
        auto [r, c] = landCells[idx];
        cellId[r][c] = idx + 1;
    }

    // ---------- Write provinces.txt (can be huge) ----------
    std::cout << "📝 Writing provinces.txt...\n";
    std::ofstream txt(outTxt);
    if (!txt) {
        std::cerr << "❌ Could not create " << outTxt << "\n";
        return 1;
    }
    for (uint32_t id = 1; id <= numProvinces; ++id) {
        auto col = colorFromID(id);
        std::string name = "Province_" + std::to_string(id);
        txt << id << ";" << name << ";" << (int)col[0] << " " << (int)col[1] << " " << (int)col[2] << "\n";
        if (id % 100000 == 0) std::cout << "  wrote " << id << " entries\r" << std::flush;
    }
    txt.close();
    std::cout << "\n";

    // ---------- Second pass: write province.bin (32-bit) and provincemap.tif ----------
    std::ofstream foutBin(outBin, std::ios::binary);
    if (!foutBin) {
        std::cerr << "❌ Could not create " << outBin << "\n";
        return 1;
    }

    // Create colour GeoTIFF
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    char** tifOpts = nullptr;
    tifOpts = CSLSetNameValue(tifOpts, "COMPRESS", "LZW");
    tifOpts = CSLSetNameValue(tifOpts, "PREDICTOR", "2");  // for Byte data
    tifOpts = CSLSetNameValue(tifOpts, "TILED", "YES");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKXSIZE", "512");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKYSIZE", "512");

    GDALDataset* tifDS = drv->Create(outTif.c_str(), width, height, 3, GDT_Byte, tifOpts);
    if (!tifDS) {
        std::cerr << "❌ Could not create " << outTif << "\n";
        return 1;
    }
    // Copy georeferencing
    double geoTransform[6];
    srcDS->GetGeoTransform(geoTransform);
    tifDS->SetGeoTransform(geoTransform);
    tifDS->SetProjection(srcDS->GetProjectionRef());

    std::vector<uint32_t> rowBin(width);
    std::vector<uint8_t> rowR(width), rowG(width), rowB(width);

    std::cout << "✍️  Second pass: writing province.bin (32-bit) and provincemap.tif...\n";

    for (int y = 0; y < height; ++y) {
        band->RasterIO(GF_Read, 0, y, width, 1, rowIn.data(), width, 1, GDT_Byte, 0, 0);
        for (int x = 0; x < width; ++x) {
            uint32_t provId = 0;
            if (rowIn[x] != 0) {
                Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
                int col = h.q + (h.r - (h.r & 1)) / 2;
                int row = h.r;
                if (col < 0) col = 0;
                if (col >= cols) col = cols - 1;
                if (row < 0) row = 0;
                if (row >= rows) row = rows - 1;
                provId = cellId[row][col];
            }
            rowBin[x] = provId;
            if (provId != 0) {
                auto col = colorFromID(provId);
                rowR[x] = col[0];
                rowG[x] = col[1];
                rowB[x] = col[2];
            } else {
                rowR[x] = 0;
                rowG[x] = 0;
                rowB[x] = 0;
            }
        }

        foutBin.write(reinterpret_cast<const char*>(rowBin.data()), width * sizeof(uint32_t));
        tifDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowR.data(), width, 1, GDT_Byte, 0, 0);
        tifDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, rowG.data(), width, 1, GDT_Byte, 0, 0);
        tifDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, rowB.data(), width, 1, GDT_Byte, 0, 0);

        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    foutBin.close();
    GDALClose(tifDS);
    GDALClose(srcDS);

    std::cout << "✅ Done!\n";
    std::cout << "  province.bin     : " << outBin << " (size ~" << (width*height*4/1024/1024) << " MB)\n";
    std::cout << "  provinces.txt    : " << outTxt << "\n";
    std::cout << "  provincemap.tif  : " << outTif << "\n";

    return 0;
}