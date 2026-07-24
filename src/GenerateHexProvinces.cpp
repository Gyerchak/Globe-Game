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

namespace fs = std::filesystem;

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 32768;

// Hexagon parameters (pointy‑top)
constexpr double HEX_RADIUS = 9.0;          // pixels (centre to corner)
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

// Convert offset (row, col) to axial (q, r)
static inline Hex offsetToAxial(int row, int col) {
    int q = col - (row - (row & 1)) / 2;
    int r = row;
    return {q, r};
}

// Convert axial to offset
static inline std::pair<int,int> axialToOffset(int q, int r) {
    int col = q + (r - (r & 1)) / 2;
    int row = r;
    return {row, col};
}

// -----------------------------------------------------------------------------
// Deterministic colour from ID (for land provinces)
// -----------------------------------------------------------------------------
static inline std::array<uint8_t, 3> colorFromID(uint32_t id) {
    uint32_t hash = id * 2654435761u;
    uint32_t rgb = hash & 0xFFFFFF;
    return {
        static_cast<uint8_t>((rgb >> 16) & 0xFF),
        static_cast<uint8_t>((rgb >> 8) & 0xFF),
        static_cast<uint8_t>(rgb & 0xFF)
    };
}

// -----------------------------------------------------------------------------
// Random unique colour generator (for sea provinces, if desired)
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

    // -------------------------------------------------------------------------
    // Phase 1: Load original 8‑bit map and create hexagonalised binary map
    // -------------------------------------------------------------------------
    GDALDataset* srcDS = (GDALDataset*)GDALOpen(inPath.c_str(), GA_ReadOnly);
    if (!srcDS) {
        std::cerr << "❌ Could not open " << inPath << "\n";
        return 1;
    }

    int width = srcDS->GetRasterXSize();
    int height = srcDS->GetRasterYSize();
    GDALRasterBand* band = srcDS->GetRasterBand(1);

    // Determine hex grid dimensions
    double xOff = 0.0, yOff = 0.0;
    int cols = static_cast<int>(std::ceil(width / DX)) + 2;
    int rows = static_cast<int>(std::ceil(height / DY)) + 2;

    std::cout << "🗺️  Hex grid: " << cols << " x " << rows << " = "
              << (cols * rows) << " cells\n";

    // Allocate arrays to store whether each hex cell is land (true) or sea (false)
    std::vector<std::vector<bool>> cellIsLand(rows, std::vector<bool>(cols, false));

    // First pass: scan every pixel, find its hex cell, mark land if pixel is land
    std::cout << "🔎 Phase 1: Hexagonalising coastline...\n";
    std::vector<uint8_t> rowIn(width);
    for (int y = 0; y < height; ++y) {
        band->RasterIO(GF_Read, 0, y, width, 1, rowIn.data(), width, 1, GDT_Byte, 0, 0);
        for (int x = 0; x < width; ++x) {
            if (rowIn[x] != 0) { // land
                Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
                int col = h.q + (h.r - (h.r & 1)) / 2;
                int row = h.r;
                if (col < 0) col = 0;
                if (col >= cols) col = cols - 1;
                if (row < 0) row = 0;
                if (row >= rows) row = rows - 1;
                cellIsLand[row][col] = true;
            }
        }
        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    // Now create the hexagonalised binary raster: for each pixel, set value = land flag of its hex cell
    // Also create a colour version for visualisation
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    char** tifOpts = nullptr;
    tifOpts = CSLSetNameValue(tifOpts, "COMPRESS", "LZW");
    tifOpts = CSLSetNameValue(tifOpts, "PREDICTOR", "2");
    tifOpts = CSLSetNameValue(tifOpts, "TILED", "YES");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKXSIZE", "512");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKYSIZE", "512");

    // Binary hexheightmap (1 band, Byte, 1=land, 0=sea)
    GDALDataset* hexDS = drv->Create(outHex.c_str(), width, height, 1, GDT_Byte, tifOpts);
    if (!hexDS) {
        std::cerr << "❌ Could not create " << outHex << "\n";
        return 1;
    }
    double geoTransform[6];
    srcDS->GetGeoTransform(geoTransform);
    hexDS->SetGeoTransform(geoTransform);
    hexDS->SetProjection(srcDS->GetProjectionRef());

    // Colour hexheightmap (3 bands, RGB)
    GDALDataset* hexColDS = drv->Create(outHexCol.c_str(), width, height, 3, GDT_Byte, tifOpts);
    if (!hexColDS) {
        std::cerr << "❌ Could not create " << outHexCol << "\n";
        return 1;
    }
    hexColDS->SetGeoTransform(geoTransform);
    hexColDS->SetProjection(srcDS->GetProjectionRef());

    std::vector<uint8_t> rowOutBin(width);
    std::vector<uint8_t> rowR(width), rowG(width), rowB(width);

    std::cout << "✍️  Writing hexheightmap.tif and hexheightmap_color.tif...\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
            int col = h.q + (h.r - (h.r & 1)) / 2;
            int row = h.r;
            if (col < 0) col = 0;
            if (col >= cols) col = cols - 1;
            if (row < 0) row = 0;
            if (row >= rows) row = rows - 1;
            bool land = cellIsLand[row][col];
            rowOutBin[x] = land ? 1 : 0;
            if (land) {
                rowR[x] = 0;   rowG[x] = 200; rowB[x] = 0;   // green for land
            } else {
                rowR[x] = 0;   rowG[x] = 0;   rowB[x] = 200; // blue for sea
            }
        }
        hexDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowOutBin.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowR.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, rowG.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, rowB.data(), width, 1, GDT_Byte, 0, 0);
        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    GDALClose(hexDS);
    GDALClose(hexColDS);
    GDALClose(srcDS);

    // Copy hexheightmap.tif to input for the next phase
    fs::path inHex = fs::current_path() / "input" / "hexheightmap.tif";
    fs::copy_file(outHex, inHex, fs::copy_options::overwrite_existing);
    std::cout << "📁 Copied hexheightmap.tif to input/ for province generation.\n";

    // -------------------------------------------------------------------------
    // Phase 2: Read hexheightmap.tif and generate provinces
    // -------------------------------------------------------------------------
    GDALDataset* hexSrcDS = (GDALDataset*)GDALOpen(inHex.c_str(), GA_ReadOnly);
    if (!hexSrcDS) {
        std::cerr << "❌ Could not open " << inHex << "\n";
        return 1;
    }

    GDALRasterBand* hexBand = hexSrcDS->GetRasterBand(1);

    // We already have cellIsLand from phase 1, but we can recompute from the hex map to be safe.
    // However, cellIsLand is already correct. We'll reuse it, but we need to assign IDs.
    // We also need to count land and sea hexes.
    std::vector<std::pair<int, int>> landCells, seaCells;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellIsLand[r][c])
                landCells.push_back({r, c});
            else
                seaCells.push_back({r, c});
        }
    }

    uint32_t numLand = static_cast<uint32_t>(landCells.size());
    uint32_t numSea  = static_cast<uint32_t>(seaCells.size());
    std::cout << "🏛️  Land hexes: " << numLand << ", Sea hexes: " << numSea << "\n";

    // Assign IDs: land = 1..numLand, sea = numLand+1 .. numLand+numSea
    std::vector<std::vector<uint32_t>> cellId(rows, std::vector<uint32_t>(cols, 0));
    uint32_t id = 1;
    for (auto [r,c] : landCells) cellId[r][c] = id++;
    for (auto [r,c] : seaCells)  cellId[r][c] = id++;

    // Generate colours for land provinces (random unique) and sea provinces (blue gradient based on distance to coast, or random)
    // We'll use random unique for both for simplicity, but for sea we could use distance-based. The user didn't specify, so we'll use random.
    std::random_device rd;
    std::mt19937 rng(rd());
    std::set<Color> usedColors;

    std::vector<Color> landPalette(numLand + 1);
    std::vector<Color> seaPalette(numSea + 1);

    // Land colours
    for (uint32_t i = 1; i <= numLand; ++i) {
        landPalette[i] = uniqueRandomColor(rng, usedColors);
    }
    // Sea colours
    for (uint32_t i = 1; i <= numSea; ++i) {
        seaPalette[i] = uniqueRandomColor(rng, usedColors);
    }

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

    // Now create province.bin (32-bit) and land/sea maps
    std::ofstream foutBin(outBin, std::ios::binary);
    if (!foutBin) {
        std::cerr << "❌ Could not create " << outBin << "\n";
        return 1;
    }

    // Create landprovincemap.tif (only land shown, sea = black)
    GDALDataset* landMapDS = drv->Create(outLandMap.c_str(), width, height, 3, GDT_Byte, tifOpts);
    if (!landMapDS) {
        std::cerr << "❌ Could not create " << outLandMap << "\n";
        return 1;
    }
    landMapDS->SetGeoTransform(geoTransform);
    landMapDS->SetProjection(hexSrcDS->GetProjectionRef());

    // Create seaprovincemap.tif (only sea shown, land = black)
    GDALDataset* seaMapDS = drv->Create(outSeaMap.c_str(), width, height, 3, GDT_Byte, tifOpts);
    if (!seaMapDS) {
        std::cerr << "❌ Could not create " << outSeaMap << "\n";
        return 1;
    }
    seaMapDS->SetGeoTransform(geoTransform);
    seaMapDS->SetProjection(hexSrcDS->GetProjectionRef());

    std::vector<uint32_t> rowBin32(width);
    std::vector<uint8_t> rowLandR(width), rowLandG(width), rowLandB(width);
    std::vector<uint8_t> rowSeaR(width), rowSeaG(width), rowSeaB(width);

    std::cout << "✍️  Phase 2: Writing province.bin, landprovincemap.tif, seaprovincemap.tif...\n";
    for (int y = 0; y < height; ++y) {
        // We can read the hexheightmap row to know land/sea, but we can also compute from cellIsLand.
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

            bool isLand = cellIsLand[row][col];
            if (isLand) {
                uint32_t landIdx = provId; // 1..numLand
                auto c = landPalette[landIdx];
                rowLandR[x] = c[0]; rowLandG[x] = c[1]; rowLandB[x] = c[2];
                rowSeaR[x] = 0; rowSeaG[x] = 0; rowSeaB[x] = 0; // black for land on sea map
            } else {
                uint32_t seaIdx = provId - numLand; // 1..numSea
                auto c = seaPalette[seaIdx];
                rowSeaR[x] = c[0]; rowSeaG[x] = c[1]; rowSeaB[x] = c[2];
                rowLandR[x] = 0; rowLandG[x] = 0; rowLandB[x] = 0; // black for sea on land map
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
    GDALClose(hexSrcDS);

    std::cout << "✅ All done!\n";
    std::cout << "  hexheightmap.tif         : " << outHex << "\n";
    std::cout << "  hexheightmap_color.tif   : " << outHexCol << "\n";
    std::cout << "  province.bin             : " << outBin << " (size ~" << (width*height*4/1024/1024) << " MB)\n";
    std::cout << "  landprovinces.txt        : " << outLandTxt << "\n";
    std::cout << "  seaprovinces.txt         : " << outSeaTxt << "\n";
    std::cout << "  landprovincemap.tif      : " << outLandMap << "\n";
    std::cout << "  seaprovincemap.tif       : " << outSeaMap << "\n";

    return 0;
}