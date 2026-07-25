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
#include <string>
#include <cstdint>
#include <chrono>
#include <unordered_map>

namespace fs = std::filesystem;

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 32768;
constexpr double HEX_RADIUS = 6.0;
constexpr double DX = std::sqrt(3.0) * HEX_RADIUS;
constexpr double DY = 1.5 * HEX_RADIUS;
constexpr uint8_t COASTAL_HEIGHT = 10;

// Helper: print with flush
template<typename... Args>
void printMsg(Args&&... args) {
    (std::cout << ... << std::forward<Args>(args)) << std::flush;
}

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

static inline Hex offsetToAxial(int row, int col) {
    int q = col - (row - (row & 1)) / 2;
    int r = row;
    return {q, r};
}

// -----------------------------------------------------------------------------
// Colour helpers
// -----------------------------------------------------------------------------
using Color = std::array<uint8_t, 3>;

static inline Color colorFromID(uint32_t id) {
    uint32_t hash = id * 2654435761u;
    uint32_t rgb = hash & 0xFFFFFF;
    return {
        static_cast<uint8_t>((rgb >> 16) & 0xFF),
        static_cast<uint8_t>((rgb >> 8) & 0xFF),
        static_cast<uint8_t>(rgb & 0xFF)
    };
}

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
    std::ios::sync_with_stdio(false);

    auto startTime = std::chrono::steady_clock::now();

    // Paths
    fs::path inPath = fs::current_path() / "input" / "heightmap_8bit.tif";
    fs::path outHex = fs::current_path() / "output" / "hexheightmap.tif";
    fs::path outHexCol = fs::current_path() / "output" / "hexheightmap_color.tif";
    fs::path outBin = fs::current_path() / "output" / "province.bin";
    fs::path outWorldMap = fs::current_path() / "output" / "worldprovincemap.tif";
    fs::path outLandMap = fs::current_path() / "output" / "landprovincemap.tif";
    fs::path outSeaMap = fs::current_path() / "output" / "seaprovincemap.tif";
    fs::path outAllTxt = fs::current_path() / "output" / "provinces.txt";
    fs::path outLandTxt = fs::current_path() / "output" / "landprovinces.txt";
    fs::path outSeaTxt = fs::current_path() / "output" / "seaprovinces.txt";

    if (!fs::exists(inPath)) {
        std::cerr << "❌ Input file not found: " << inPath << "\n";
        return 1;
    }
    printMsg("✅ Input found: ", inPath.string(), "\n");
    fs::create_directories(outHex.parent_path());

    GDALDataset* srcDS = (GDALDataset*)GDALOpen(inPath.c_str(), GA_ReadOnly);
    if (!srcDS) {
        std::cerr << "❌ Could not open " << inPath << "\n";
        return 1;
    }
    printMsg("✅ Opened source dataset\n");

    int width = srcDS->GetRasterXSize();
    int height = srcDS->GetRasterYSize();
    GDALRasterBand* band = srcDS->GetRasterBand(1);

    double xOff = 0.0, yOff = 0.0;
    int cols = static_cast<int>(std::ceil(width / DX)) + 2;
    int rows = static_cast<int>(std::ceil(height / DY)) + 2;
    printMsg("🗺️  Hex grid (radius 6): ", cols, " x ", rows, " = ", (cols * rows), " cells\n");

    // ---------- Phase 1: accumulate statistics ----------
    std::vector<std::vector<uint64_t>> sumLand(rows, std::vector<uint64_t>(cols, 0));
    std::vector<std::vector<uint32_t>> countLand(rows, std::vector<uint32_t>(cols, 0));
    std::vector<std::vector<uint32_t>> countSea(rows, std::vector<uint32_t>(cols, 0));
    std::vector<uint8_t> rowIn(width);

    printMsg("🔎 Phase 1: Scanning pixels...\n");
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
        if (y % 10000 == 0) printMsg("  row ", y, " / ", height, "\n");
    }
    printMsg("✅ Phase 1 done.\n");

    // ---------- Classify hexes ----------
    std::vector<std::vector<bool>> cellIsLand(rows, std::vector<bool>(cols, false));
    std::vector<std::vector<uint8_t>> cellHeight(rows, std::vector<uint8_t>(cols, 0));
    std::vector<std::vector<bool>> cellIsCoastal(rows, std::vector<bool>(cols, false));

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (countLand[r][c] > 0 && countSea[r][c] == 0) {
                cellIsLand[r][c] = true;
                cellIsCoastal[r][c] = false;
                uint8_t avg = static_cast<uint8_t>(sumLand[r][c] / countLand[r][c]);
                cellHeight[r][c] = avg;
            } else if (countLand[r][c] == 0 && countSea[r][c] > 0) {
                cellIsLand[r][c] = false;
                cellIsCoastal[r][c] = false;
                cellHeight[r][c] = 0;
            } else if (countLand[r][c] > 0 && countSea[r][c] > 0) {
                cellIsLand[r][c] = true;
                cellIsCoastal[r][c] = true;
                cellHeight[r][c] = COASTAL_HEIGHT;
            } else {
                cellIsLand[r][c] = false;
                cellIsCoastal[r][c] = false;
                cellHeight[r][c] = 0;
            }
        }
    }
    printMsg("✅ Classification done.\n");

    // ---------- Write hexheightmap.tif and hexheightmap_color.tif ----------
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    char** tifOpts = nullptr;
    tifOpts = CSLSetNameValue(tifOpts, "COMPRESS", "LZW");
    tifOpts = CSLSetNameValue(tifOpts, "PREDICTOR", "2");
    tifOpts = CSLSetNameValue(tifOpts, "TILED", "YES");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKXSIZE", "512");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKYSIZE", "512");

    GDALDataset* hexDS = drv->Create(outHex.c_str(), width, height, 1, GDT_Byte, tifOpts);
    if (!hexDS) { std::cerr << "❌ Could not create " << outHex << "\n"; return 1; }
    GDALDataset* hexColDS = drv->Create(outHexCol.c_str(), width, height, 3, GDT_Byte, tifOpts);
    if (!hexColDS) { std::cerr << "❌ Could not create " << outHexCol << "\n"; return 1; }

    double geoTransform[6];
    srcDS->GetGeoTransform(geoTransform);
    hexDS->SetGeoTransform(geoTransform);
    hexDS->SetProjection(srcDS->GetProjectionRef());
    hexColDS->SetGeoTransform(geoTransform);
    hexColDS->SetProjection(srcDS->GetProjectionRef());

    auto palette = buildPalette();
    std::vector<uint8_t> rowHt(width), rowR(width), rowG(width), rowB(width);

    printMsg("✍️  Writing hexheightmap.tif and hexheightmap_color.tif...\n");
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
            auto c = palette[ht];
            rowR[x] = c[0];
            rowG[x] = c[1];
            rowB[x] = c[2];
        }
        hexDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowHt.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowR.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, rowG.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, rowB.data(), width, 1, GDT_Byte, 0, 0);
        if (y % 10000 == 0) printMsg("  row ", y, " / ", height, "\n");
    }
    GDALClose(hexDS);
    GDALClose(hexColDS);
    printMsg("✅ hexheightmap files written.\n");

    // Copy hexheightmap.tif to input/
    fs::path inHex = fs::current_path() / "input" / "hexheightmap.tif";
    fs::copy_file(outHex, inHex, fs::copy_options::overwrite_existing);
    printMsg("📁 Copied hexheightmap.tif to input/\n");

    // ---------- Phase 2: Province generation with east‑west wrap merging ----------
    std::vector<std::pair<int,int>> landCells, seaCells;
    landCells.reserve(rows * cols / 2);
    seaCells.reserve(rows * cols / 2);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellIsLand[r][c] || cellIsCoastal[r][c]) {
                landCells.emplace_back(r, c);
            } else {
                seaCells.emplace_back(r, c);
            }
        }
    }

    // Initial assignment of IDs
    std::vector<std::vector<uint32_t>> cellId(rows, std::vector<uint32_t>(cols, 0));
    uint32_t id = 1;
    for (auto &p : landCells) cellId[p.first][p.second] = id++;
    for (auto &p : seaCells)  cellId[p.first][p.second] = id++;
    printMsg("🔗 Initial IDs assigned: ", (id-1), "\n");

    // Merge east‑west edge hexes
    printMsg("🔄 Merging east‑west edge hexes...\n");
    int mergedCount = 0;
    for (int r = 0; r < rows; ++r) {
        int leftId = cellId[r][0];
        int rightId = cellId[r][cols-1];
        if (leftId == 0 || rightId == 0) continue;
        bool leftIsLand = cellIsLand[r][0] || cellIsCoastal[r][0];
        bool rightIsLand = cellIsLand[r][cols-1] || cellIsCoastal[r][cols-1];
        if (leftIsLand != rightIsLand) continue;
        Hex leftAx = offsetToAxial(r, 0);
        Hex rightAx = offsetToAxial(r, cols-1);
        Hex eastNeighbor = {rightAx.q + 1, rightAx.r};
        auto [nr, nc] = axialToOffset(eastNeighbor.q, eastNeighbor.r);
        if (nr == r && nc == 0) {
            uint32_t minId = std::min(leftId, rightId);
            uint32_t maxId = std::max(leftId, rightId);
            for (int rr = 0; rr < rows; ++rr) {
                for (int cc = 0; cc < cols; ++cc) {
                    if (cellId[rr][cc] == maxId) {
                        cellId[rr][cc] = minId;
                    }
                }
            }
            mergedCount++;
        }
    }
    printMsg("✅ Merged ", mergedCount, " edge pairs.\n");

    // Renumber IDs to be contiguous
    printMsg("🔢 Renumbering IDs to be contiguous...\n");
    std::unordered_map<uint32_t, uint32_t> oldToNew;
    uint32_t newId = 1;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            uint32_t old = cellId[r][c];
            if (old == 0) continue;
            auto it = oldToNew.find(old);
            if (it == oldToNew.end()) {
                oldToNew[old] = newId++;
            }
        }
    }
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellId[r][c] != 0) {
                cellId[r][c] = oldToNew[cellId[r][c]];
            }
        }
    }
    uint32_t totalProvinces = newId - 1;
    printMsg("✅ Total provinces after merging and renumbering: ", totalProvinces, "\n");

    // ---------- Pre‑compute centre pixel for each hex ----------
    std::vector<std::vector<bool>> isCentre(rows, std::vector<bool>(cols, false));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellId[r][c] == 0) continue;
            Hex ax = offsetToAxial(r, c);
            auto [cx, cy] = hexToPixel(ax.q, ax.r, xOff, yOff);
            int px = static_cast<int>(std::round(cx));
            int py = static_cast<int>(std::round(cy));
            if (px >= 0 && px < width && py >= 0 && py < height) {
                isCentre[r][c] = true;
            }
        }
    }

    // ---------- Write text files (ID;R G B A) with A=0 ----------
    printMsg("📝 Writing provinces.txt (combined)...\n");
    std::ofstream allTxt(outAllTxt);
    if (!allTxt) { std::cerr << "❌ Cannot create " << outAllTxt << "\n"; return 1; }
    for (uint32_t i = 1; i <= totalProvinces; ++i) {
        auto c = colorFromID(i);
        allTxt << i << ";" << (int)c[0] << " " << (int)c[1] << " " << (int)c[2] << " 0\n";
        if (i % 100000 == 0) printMsg("  wrote ", i, " entries\n");
    }
    allTxt.close();
    printMsg("✅ provinces.txt written.\n");

    printMsg("📝 Writing landprovinces.txt...\n");
    std::ofstream ltxt(outLandTxt);
    if (!ltxt) { std::cerr << "❌ Cannot create " << outLandTxt << "\n"; return 1; }
    uint32_t landCount = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellIsLand[r][c] || cellIsCoastal[r][c]) {
                uint32_t id = cellId[r][c];
                if (id > 0) {
                    landCount++;
                    auto col = colorFromID(id);
                    ltxt << id << ";" << (int)col[0] << " " << (int)col[1] << " " << (int)col[2] << " 0\n";
                    if (landCount % 100000 == 0) printMsg("  wrote ", landCount, " land entries\n");
                }
            }
        }
    }
    ltxt.close();
    printMsg("✅ landprovinces.txt written (", landCount, " entries).\n");

    printMsg("📝 Writing seaprovinces.txt...\n");
    std::ofstream stxt(outSeaTxt);
    if (!stxt) { std::cerr << "❌ Cannot create " << outSeaTxt << "\n"; return 1; }
    uint32_t seaCount = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!(cellIsLand[r][c] || cellIsCoastal[r][c])) {
                uint32_t id = cellId[r][c];
                if (id > 0) {
                    seaCount++;
                    auto col = colorFromID(id);
                    stxt << id << ";" << (int)col[0] << " " << (int)col[1] << " " << (int)col[2] << " 0\n";
                    if (seaCount % 100000 == 0) printMsg("  wrote ", seaCount, " sea entries\n");
                }
            }
        }
    }
    stxt.close();
    printMsg("✅ seaprovinces.txt written (", seaCount, " entries).\n");

    // ---------- Write province.bin and the three RGBA maps ----------
    printMsg("✍️  Writing province.bin (ID<<1 | centre_flag), worldprovincemap.tif, landprovincemap.tif, seaprovincemap.tif (RGBA with alpha encoding)...\n");
    std::ofstream foutBin(outBin, std::ios::binary);
    if (!foutBin) { std::cerr << "❌ Cannot create " << outBin << "\n"; return 1; }

    // RGBA options
    char** rgbaOpts = nullptr;
    rgbaOpts = CSLSetNameValue(rgbaOpts, "COMPRESS", "LZW");
    rgbaOpts = CSLSetNameValue(rgbaOpts, "PREDICTOR", "2");
    rgbaOpts = CSLSetNameValue(rgbaOpts, "TILED", "YES");
    rgbaOpts = CSLSetNameValue(rgbaOpts, "BLOCKXSIZE", "512");
    rgbaOpts = CSLSetNameValue(rgbaOpts, "BLOCKYSIZE", "512");
    rgbaOpts = CSLSetNameValue(rgbaOpts, "PHOTOMETRIC", "RGB");

    GDALDataset* worldMapDS = drv->Create(outWorldMap.c_str(), width, height, 4, GDT_Byte, rgbaOpts);
    if (!worldMapDS) { std::cerr << "❌ Cannot create " << outWorldMap << "\n"; return 1; }
    worldMapDS->SetGeoTransform(geoTransform);
    worldMapDS->SetProjection(srcDS->GetProjectionRef());

    GDALDataset* landMapDS = drv->Create(outLandMap.c_str(), width, height, 4, GDT_Byte, rgbaOpts);
    if (!landMapDS) { std::cerr << "❌ Cannot create " << outLandMap << "\n"; return 1; }
    landMapDS->SetGeoTransform(geoTransform);
    landMapDS->SetProjection(srcDS->GetProjectionRef());

    GDALDataset* seaMapDS = drv->Create(outSeaMap.c_str(), width, height, 4, GDT_Byte, rgbaOpts);
    if (!seaMapDS) { std::cerr << "❌ Cannot create " << outSeaMap << "\n"; return 1; }
    seaMapDS->SetGeoTransform(geoTransform);
    seaMapDS->SetProjection(srcDS->GetProjectionRef());

    std::vector<uint32_t> rowBin32(width);
    std::vector<uint8_t> wR(width), wG(width), wB(width), wA(width);
    std::vector<uint8_t> lR(width), lG(width), lB(width), lA(width);
    std::vector<uint8_t> sR(width), sG(width), sB(width), sA(width);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
            int col = h.q + (h.r - (h.r & 1)) / 2;
            int row = h.r;
            if (col < 0) col = 0;
            if (col >= cols) col = cols - 1;
            if (row < 0) row = 0;
            if (row >= rows) row = rows - 1;
            uint32_t id = cellId[row][col];
            bool centre = isCentre[row][col];
            uint32_t packed = (id << 1) | (centre ? 1 : 0);
            rowBin32[x] = packed;

            auto c = colorFromID(id);
            bool isLand = cellIsLand[row][col] || cellIsCoastal[row][col];

            // Alpha encoding:
            // centre → 255 (fully transparent)
            // land non‑centre → 0
            // sea non‑centre → 128 (half transparent)
            uint8_t alpha;
            if (centre) {
                alpha = 255;
            } else {
                alpha = isLand ? 0 : 128;
            }

            // World map: always show colour with appropriate alpha
            wR[x] = c[0]; wG[x] = c[1]; wB[x] = c[2]; wA[x] = alpha;

            // Land map: only land gets colour, sea black
            if (isLand) {
                lR[x] = c[0]; lG[x] = c[1]; lB[x] = c[2]; lA[x] = alpha;
                sR[x] = 0; sG[x] = 0; sB[x] = 0; sA[x] = 0;
            } else {
                lR[x] = 0; lG[x] = 0; lB[x] = 0; lA[x] = 0;
                sR[x] = c[0]; sG[x] = c[1]; sB[x] = c[2]; sA[x] = alpha;
            }
        }

        foutBin.write(reinterpret_cast<const char*>(rowBin32.data()), width * sizeof(uint32_t));
        worldMapDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, wR.data(), width, 1, GDT_Byte, 0, 0);
        worldMapDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, wG.data(), width, 1, GDT_Byte, 0, 0);
        worldMapDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, wB.data(), width, 1, GDT_Byte, 0, 0);
        worldMapDS->GetRasterBand(4)->RasterIO(GF_Write, 0, y, width, 1, wA.data(), width, 1, GDT_Byte, 0, 0);
        landMapDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, lR.data(), width, 1, GDT_Byte, 0, 0);
        landMapDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, lG.data(), width, 1, GDT_Byte, 0, 0);
        landMapDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, lB.data(), width, 1, GDT_Byte, 0, 0);
        landMapDS->GetRasterBand(4)->RasterIO(GF_Write, 0, y, width, 1, lA.data(), width, 1, GDT_Byte, 0, 0);
        seaMapDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, sR.data(), width, 1, GDT_Byte, 0, 0);
        seaMapDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, sG.data(), width, 1, GDT_Byte, 0, 0);
        seaMapDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, sB.data(), width, 1, GDT_Byte, 0, 0);
        seaMapDS->GetRasterBand(4)->RasterIO(GF_Write, 0, y, width, 1, sA.data(), width, 1, GDT_Byte, 0, 0);

        if (y % 10000 == 0) printMsg("  row ", y, " / ", height, "\n");
    }

    foutBin.close();
    GDALClose(worldMapDS);
    GDALClose(landMapDS);
    GDALClose(seaMapDS);
    GDALClose(srcDS);

    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    printMsg("\n🎉 All done in ", elapsed, " seconds!\n");
    printMsg("  hexheightmap.tif         : ", outHex.string(), "\n");
    printMsg("  hexheightmap_color.tif   : ", outHexCol.string(), "\n");
    printMsg("  province.bin             : ", outBin.string(), " (~", (width*height*4/1024/1024), " MB)\n");
    printMsg("  worldprovincemap.tif     : ", outWorldMap.string(), " (RGBA)\n");
    printMsg("  landprovincemap.tif      : ", outLandMap.string(), " (RGBA)\n");
    printMsg("  seaprovincemap.tif       : ", outSeaMap.string(), " (RGBA)\n");
    printMsg("  provinces.txt            : ", outAllTxt.string(), " (ID;R G B A)\n");
    printMsg("  landprovinces.txt        : ", outLandTxt.string(), " (ID;R G B A)\n");
    printMsg("  seaprovinces.txt         : ", outSeaTxt.string(), " (ID;R G B A)\n");

    return 0;
}