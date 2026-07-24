// GenerateHexProvinces.cpp
// Compile: g++ -std=c++20 -O3 GenerateHexProvinces.cpp -o exe/GenerateHexProvinces -lgdal
// Run: ./exe/GenerateHexProvinces

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
#include <limits>
#include <set>

namespace fs = std::filesystem;

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 32768;
constexpr double HEX_RADIUS = 9.0;
constexpr double DX = std::sqrt(3.0) * HEX_RADIUS;
constexpr double DY = 1.5 * HEX_RADIUS;
constexpr uint8_t COASTAL_HEIGHT = 10;

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

static inline std::pair<int,int> axialToOffset(int q, int r) {
    int col = q + (r - (r & 1)) / 2;
    int row = r;
    return {row, col};
}

// -----------------------------------------------------------------------------
// Color helpers
// -----------------------------------------------------------------------------
using Color = std::array<uint8_t, 3>;

static Color randomColor(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 255);
    return { static_cast<uint8_t>(dist(rng)),
             static_cast<uint8_t>(dist(rng)),
             static_cast<uint8_t>(dist(rng)) };
}

static Color uniqueRandomColor(std::mt19937& rng, std::set<Color>& used) {
    Color col;
    do {
        col = randomColor(rng);
    } while (used.find(col) != used.end() || (col[0] + col[1] + col[2]) < 60);
    used.insert(col);
    return col;
}

// -----------------------------------------------------------------------------
// Build the exact colour palette from the original 26 control points
// -----------------------------------------------------------------------------
static std::array<Color, 256> buildPalette() {
    struct Point { int val; uint8_t r,g,b; };
    std::vector<Point> pts = {
        {0,   0,   0,   255},
        {1,   0,   200, 100},
        {11,  0,   153, 50},
        {22,  0,   100, 0},
        {33,  64,  114, 0},
        {43,  128, 128, 0},
        {54,  64,  191, 0},
        {65,  0,   255, 0},
        {75,  128, 255, 0},
        {86,  255, 255, 0},
        {96,  191, 255, 0},
        {107, 127, 255, 0},
        {117, 191, 210, 0},
        {128, 255, 165, 0},
        {138, 255, 82,  0},
        {149, 255, 0,   0},
        {160, 255, 96,  101},
        {170, 255, 192, 203},
        {181, 191, 96,  165},
        {192, 128, 0,   128},
        {202, 64,  0,   64},
        {213, 0,   0,   0},
        {223, 64,  64,  64},
        {234, 128, 128, 128},
        {244, 191, 191, 191},
        {255, 255, 255, 255}
    };
    std::array<Color, 256> pal{};
    for (size_t i = 0; i < pts.size(); ++i) {
        int startVal = pts[i].val;
        int endVal = (i+1 < pts.size()) ? pts[i+1].val : 255;
        if (endVal <= startVal) continue;
        for (int v = startVal; v <= endVal; ++v) {
            float t = (v - startVal) / (float)(endVal - startVal);
            uint8_t r = static_cast<uint8_t>(pts[i].r + t * (pts[i+1].r - pts[i].r));
            uint8_t g = static_cast<uint8_t>(pts[i].g + t * (pts[i+1].g - pts[i].g));
            uint8_t b = static_cast<uint8_t>(pts[i].b + t * (pts[i+1].b - pts[i].b));
            pal[v] = {r,g,b};
        }
    }
    return pal;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    GDALAllRegister();

    fs::path inPath = fs::current_path() / "input" / "heightmap_8bit.tif";
    fs::path outHex = fs::current_path() / "output" / "hexheightmap.tif";
    fs::path outHexCol = fs::current_path() / "output" / "hexheightmap_color.tif";
    fs::path outBin = fs::current_path() / "output" / "province.bin";
    fs::path outLandTxt = fs::current_path() / "output" / "landprovinces.txt";
    fs::path outSeaTxt = fs::current_path() / "output" / "seaprovinces.txt";
    fs::path outLandMap = fs::current_path() / "output" / "landprovincemap.tif";
    fs::path outSeaMap = fs::current_path() / "output" / "seaprovincemap.tif";

    if (!fs::exists(inPath)) {
        std::cerr << "❌ Input heightmap not found: " << inPath << "\n";
        return 1;
    }

    fs::create_directories(outHex.parent_path());

    // Build the palette for the height colour map
    auto palette = buildPalette();

    // -------------------------------------------------------------------------
    // Phase 1: Load original 8‑bit map, compute average height per hex,
    //          and classify as interior land, coastal, or sea.
    // -------------------------------------------------------------------------
    GDALDataset* srcDS = (GDALDataset*)GDALOpen(inPath.c_str(), GA_ReadOnly);
    if (!srcDS) {
        std::cerr << "❌ Could not open " << inPath << "\n";
        return 1;
    }

    int width = srcDS->GetRasterXSize();
    int height = srcDS->GetRasterYSize();
    GDALRasterBand* band = srcDS->GetRasterBand(1);

    double xOff = 0.0, yOff = 0.0;
    int cols = static_cast<int>(std::ceil(width / DX)) + 2;
    int rows = static_cast<int>(std::ceil(height / DY)) + 2;

    std::cout << "🗺️  Hex grid: " << cols << " x " << rows << " = "
              << (cols * rows) << " cells\n";

    // Structures to accumulate sums and counts per hex
    std::vector<std::vector<uint64_t>> sumLand(rows, std::vector<uint64_t>(cols, 0));
    std::vector<std::vector<uint32_t>> countLand(rows, std::vector<uint32_t>(cols, 0));
    std::vector<std::vector<uint32_t>> countSea(rows, std::vector<uint32_t>(cols, 0));

    std::vector<uint8_t> rowIn(width);

    std::cout << "🔎 Phase 1: Scanning pixels to compute hex statistics...\n";
    for (int y = 0; y < height; ++y) {
        band->RasterIO(GF_Read, 0, y, width, 1, rowIn.data(), width, 1, GDT_Byte, 0, 0);
        for (int x = 0; x < width; ++x) {
            uint8_t val = rowIn[x];
            Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
            int col = h.q + (h.r - (h.r & 1)) / 2;
            int row = h.r;
            if (col < 0) col = 0;
            if (col >= cols) col = cols - 1;
            if (row < 0) row = 0;
            if (row >= rows) row = rows - 1;
            if (val > 0) {
                sumLand[row][col] += val;
                countLand[row][col]++;
            } else {
                countSea[row][col]++;
            }
        }
        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    // Now compute for each hex: interior land (no sea), sea (no land), coastal (both)
    std::vector<std::vector<bool>> cellIsLand(rows, std::vector<bool>(cols, false));
    std::vector<std::vector<uint8_t>> cellHeight(rows, std::vector<uint8_t>(cols, 0));
    std::vector<std::vector<bool>> cellIsCoastal(rows, std::vector<bool>(cols, false));

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (countLand[r][c] > 0 && countSea[r][c] == 0) {
                // Interior land
                cellIsLand[r][c] = true;
                cellIsCoastal[r][c] = false;
                uint8_t avg = static_cast<uint8_t>(sumLand[r][c] / countLand[r][c]);
                cellHeight[r][c] = avg;
            } else if (countLand[r][c] == 0 && countSea[r][c] > 0) {
                // Pure sea
                cellIsLand[r][c] = false;
                cellIsCoastal[r][c] = false;
                cellHeight[r][c] = 0;
            } else if (countLand[r][c] > 0 && countSea[r][c] > 0) {
                // Coastal – treat as land for province purposes
                cellIsLand[r][c] = true;
                cellIsCoastal[r][c] = true;
                cellHeight[r][c] = COASTAL_HEIGHT;
            } else {
                // Empty (should not happen)
                cellIsLand[r][c] = false;
                cellIsCoastal[r][c] = false;
                cellHeight[r][c] = 0;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Write hexheightmap.tif (height values) and hexheightmap_color.tif (using palette)
    // -------------------------------------------------------------------------
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    char** tifOpts = nullptr;
    tifOpts = CSLSetNameValue(tifOpts, "COMPRESS", "LZW");
    tifOpts = CSLSetNameValue(tifOpts, "PREDICTOR", "2");
    tifOpts = CSLSetNameValue(tifOpts, "TILED", "YES");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKXSIZE", "512");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKYSIZE", "512");

    GDALDataset* hexDS = drv->Create(outHex.c_str(), width, height, 1, GDT_Byte, tifOpts);
    if (!hexDS) {
        std::cerr << "❌ Could not create " << outHex << "\n";
        return 1;
    }
    double geoTransform[6];
    srcDS->GetGeoTransform(geoTransform);
    hexDS->SetGeoTransform(geoTransform);
    hexDS->SetProjection(srcDS->GetProjectionRef());

    GDALDataset* hexColDS = drv->Create(outHexCol.c_str(), width, height, 3, GDT_Byte, tifOpts);
    if (!hexColDS) {
        std::cerr << "❌ Could not create " << outHexCol << "\n";
        return 1;
    }
    hexColDS->SetGeoTransform(geoTransform);
    hexColDS->SetProjection(srcDS->GetProjectionRef());

    std::vector<uint8_t> rowHt(width);
    std::vector<uint8_t> rowR(width), rowG(width), rowB(width);

    std::cout << "✍️  Writing hexheightmap.tif and hexheightmap_color.tif (using original palette)...\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
            int col = h.q + (h.r - (h.r & 1)) / 2;
            int row = h.r;
            if (col < 0) col = 0;
            if (col >= cols) col = cols - 1;
            if (row < 0) row = 0;
            if (row >= rows) row = rows - 1;
            uint8_t ht = cellHeight[row][col];
            rowHt[x] = ht;
            // Apply the same colour palette as the original height map
            auto c = palette[ht];
            rowR[x] = c[0];
            rowG[x] = c[1];
            rowB[x] = c[2];
        }
        hexDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowHt.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowR.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, rowG.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, rowB.data(), width, 1, GDT_Byte, 0, 0);
        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    GDALClose(hexDS);
    GDALClose(hexColDS);
    GDALClose(srcDS);

    // Copy hexheightmap.tif to input for province generation
    fs::path inHex = fs::current_path() / "input" / "hexheightmap.tif";
    fs::copy_file(outHex, inHex, fs::copy_options::overwrite_existing);
    std::cout << "📁 Copied hexheightmap.tif to input/ for province generation.\n";

    // -------------------------------------------------------------------------
    // Phase 2: Generate provinces using the hex grid information we already have
    // -------------------------------------------------------------------------
    // We have cellIsLand, cellIsCoastal, cellHeight, and the grid dimensions.
    // For provinces, we assign a unique ID to every hex cell (land and sea).

    std::vector<std::pair<int, int>> landCells, seaCells;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellIsLand[r][c] || cellIsCoastal[r][c])
                landCells.push_back({r, c});
            else
                seaCells.push_back({r, c});
        }
    }

    uint32_t numLand = static_cast<uint32_t>(landCells.size());
    uint32_t numSea  = static_cast<uint32_t>(seaCells.size());
    std::cout << "🏛️  Land hexes (including coastal): " << numLand
              << ", Sea hexes: " << numSea << "\n";

    // Assign IDs: land = 1..numLand, sea = numLand+1..numLand+numSea
    std::vector<std::vector<uint32_t>> cellId(rows, std::vector<uint32_t>(cols, 0));
    uint32_t id = 1;
    for (auto [r,c] : landCells) cellId[r][c] = id++;
    for (auto [r,c] : seaCells)  cellId[r][c] = id++;

    // Generate unique colours for land provinces and sea provinces
    std::random_device rd;
    std::mt19937 rng(rd());
    std::set<Color> usedColors;

    std::vector<Color> landPalette(numLand + 1);
    std::vector<Color> seaPalette(numSea + 1);

    for (uint32_t i = 1; i <= numLand; ++i) landPalette[i] = uniqueRandomColor(rng, usedColors);
    for (uint32_t i = 1; i <= numSea; ++i)  seaPalette[i]  = uniqueRandomColor(rng, usedColors);

    // Write landprovinces.txt
    std::cout << "📝 Writing landprovinces.txt...\n";
    std::ofstream ltxt(outLandTxt);
    if (!ltxt) {
        std::cerr << "❌ Could not create " << outLandTxt << "\n";
        return 1;
    }
    for (uint32_t i = 1; i <= numLand; ++i) {
        auto c = landPalette[i];
        ltxt << i << ";Province_Land_" << i << ";" << (int)c[0] << " " << (int)c[1] << " " << (int)c[2] << "\n";
    }
    ltxt.close();

    // Write seaprovinces.txt
    std::cout << "📝 Writing seaprovinces.txt...\n";
    std::ofstream stxt(outSeaTxt);
    if (!stxt) {
        std::cerr << "❌ Could not create " << outSeaTxt << "\n";
        return 1;
    }
    for (uint32_t i = 1; i <= numSea; ++i) {
        auto c = seaPalette[i];
        uint32_t globalId = numLand + i;
        stxt << globalId << ";Province_Sea_" << i << ";" << (int)c[0] << " " << (int)c[1] << " " << (int)c[2] << "\n";
    }
    stxt.close();

    // Create province.bin (32-bit)
    std::ofstream foutBin(outBin, std::ios::binary);
    if (!foutBin) {
        std::cerr << "❌ Could not create " << outBin << "\n";
        return 1;
    }

    // Create landprovincemap.tif and seaprovincemap.tif
    GDALDataset* landMapDS = drv->Create(outLandMap.c_str(), width, height, 3, GDT_Byte, tifOpts);
    if (!landMapDS) {
        std::cerr << "❌ Could not create " << outLandMap << "\n";
        return 1;
    }
    landMapDS->SetGeoTransform(geoTransform);
    landMapDS->SetProjection(srcDS->GetProjectionRef());

    GDALDataset* seaMapDS = drv->Create(outSeaMap.c_str(), width, height, 3, GDT_Byte, tifOpts);
    if (!seaMapDS) {
        std::cerr << "❌ Could not create " << outSeaMap << "\n";
        return 1;
    }
    seaMapDS->SetGeoTransform(geoTransform);
    seaMapDS->SetProjection(srcDS->GetProjectionRef());

    std::vector<uint32_t> rowBin32(width);
    std::vector<uint8_t> rowLandR(width), rowLandG(width), rowLandB(width);
    std::vector<uint8_t> rowSeaR(width), rowSeaG(width), rowSeaB(width);

    std::cout << "✍️  Phase 2: Writing province.bin, landprovincemap.tif, seaprovincemap.tif...\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
            int col = h.q + (h.r - (h.r & 1)) / 2;
            int row = h.r;
            if (col < 0) col = 0;
            if (col >= cols) col = cols - 1;
            if (row < 0) row = 0;
            if (row >= rows) row = rows - 1;
            uint32_t provId = cellId[row][col];
            rowBin32[x] = provId;

            bool isLand = cellIsLand[row][col] || cellIsCoastal[row][col];
            if (isLand) {
                // Land province
                uint32_t landIdx = provId; // since land IDs start at 1
                auto c = landPalette[landIdx];
                rowLandR[x] = c[0]; rowLandG[x] = c[1]; rowLandB[x] = c[2];
                // Sea map: black for land
                rowSeaR[x] = 0; rowSeaG[x] = 0; rowSeaB[x] = 0;
            } else {
                // Sea province
                uint32_t seaIdx = provId - numLand; // 1..numSea
                auto c = seaPalette[seaIdx];
                rowSeaR[x] = c[0]; rowSeaG[x] = c[1]; rowSeaB[x] = c[2];
                rowLandR[x] = 0; rowLandG[x] = 0; rowLandB[x] = 0;
            }
        }

        foutBin.write(reinterpret_cast<const char*>(rowBin32.data()), width * sizeof(uint32_t));
        landMapDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowLandR.data(), width, 1, GDT_Byte, 0, 0);
        landMapDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, rowLandG.data(), width, 1, GDT_Byte, 0, 0);
        landMapDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, rowLandB.data(), width, 1, GDT_Byte, 0, 0);
        seaMapDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowSeaR.data(), width, 1, GDT_Byte, 0, 0);
        seaMapDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, rowSeaG.data(), width, 1, GDT_Byte, 0, 0);
        seaMapDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, rowSeaB.data(), width, 1, GDT_Byte, 0, 0);

        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    foutBin.close();
    GDALClose(landMapDS);
    GDALClose(seaMapDS);

    std::cout << "✅ All done!\n";
    std::cout << "  hexheightmap.tif         : " << outHex << "\n";
    std::cout << "  hexheightmap_color.tif   : " << outHexCol << "\n";
    std::cout << "  province.bin             : " << outBin << "\n";
    std::cout << "  landprovinces.txt        : " << outLandTxt << "\n";
    std::cout << "  seaprovinces.txt         : " << outSeaTxt << "\n";
    std::cout << "  landprovincemap.tif      : " << outLandMap << "\n";
    std::cout << "  seaprovincemap.tif       : " << outSeaMap << "\n";

    return 0;
}