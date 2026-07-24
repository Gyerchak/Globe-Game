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
#include <queue>
#include <string>
#include <cstdint>
#include <limits>

namespace fs = std::filesystem;

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 32768;

// Hexagon parameters (pointy‑top)
constexpr double HEX_RADIUS = 6.0;          // pixels (centre to corner)
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
// Convert offset (row, col) to axial (q, r)
// -----------------------------------------------------------------------------
static inline Hex offsetToAxial(int row, int col) {
    int q = col - (row - (row & 1)) / 2;
    int r = row;
    return {q, r};
}

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
// Colour from distance level (for sea)
// -----------------------------------------------------------------------------
static inline std::array<uint8_t, 3> colorFromDistance(int distLevel, int maxLevel) {
    float t = (maxLevel > 0) ? static_cast<float>(distLevel) / maxLevel : 0.0f;
    // from light blue (coastal) to dark blue (deep)
    uint8_t r = static_cast<uint8_t>(20 + 80 * (1.0f - t));
    uint8_t g = static_cast<uint8_t>(100 + 100 * (1.0f - t));
    uint8_t b = static_cast<uint8_t>(200 + 50 * t);
    return {r, g, b};
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    GDALAllRegister();

    fs::path inHeight = fs::current_path() / "input" / "heightmap_8bit.tif";
    fs::path outBin = fs::current_path() / "output" / "province.bin";
    fs::path outLandTxt = fs::current_path() / "output" / "provinces.txt";
    fs::path outSeaTxt = fs::current_path() / "output" / "seaprovinces.txt";
    fs::path outCombinedTif = fs::current_path() / "output" / "provincemap.tif";
    fs::path outSeaTif = fs::current_path() / "output" / "seaprovinces.tif";

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

    // ---------- First pass: mark land cells and also detect sea cells ----------
    std::cout << "🔎 First pass: scanning for land/sea cells...\n";
    std::vector<uint8_t> rowIn(width);
    std::vector<std::vector<bool>> cellIsLand(rows, std::vector<bool>(cols, false));

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

    // ---------- Collect land cells and assign land IDs ----------
    std::vector<std::pair<int, int>> landCells;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellIsLand[r][c]) landCells.push_back({r, c});
        }
    }

    uint32_t numLandProvinces = static_cast<uint32_t>(landCells.size());
    std::cout << "🏛️  Number of land provinces: " << numLandProvinces << "\n";

    std::vector<std::vector<uint32_t>> cellId(rows, std::vector<uint32_t>(cols, 0));
    for (uint32_t idx = 0; idx < numLandProvinces; ++idx) {
        auto [r, c] = landCells[idx];
        cellId[r][c] = idx + 1;   // 1..numLandProvinces
    }

    // ---------- BFS distance transform on hex grid for sea cells ----------
    // We'll compute distance (in hex steps) from each sea cell to nearest land cell.
    std::vector<std::vector<int>> dist(rows, std::vector<int>(cols, std::numeric_limits<int>::max()));
    std::queue<std::pair<int, int>> q;

    // Initialize with land cells (distance 0)
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellIsLand[r][c]) {
                dist[r][c] = 0;
                q.push({r, c});
            }
        }
    }

    // BFS on 6 neighbors
    int maxDist = 0;
    while (!q.empty()) {
        auto [r, c] = q.front(); q.pop();
        Hex h = offsetToAxial(r, c);
        // 6 neighbors in axial coords: (q+1,r), (q-1,r), (q,r+1), (q,r-1), (q+1,r-1), (q-1,r+1)
        int neigh[6][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}, {1,-1}, {-1,1}};
        for (auto& d : neigh) {
            int nq = h.q + d[0];
            int nr = h.r + d[1];
            auto [nr2, nc] = axialToOffset(nq, nr);
            if (nr2 < 0 || nr2 >= rows || nc < 0 || nc >= cols) continue;
            if (dist[nr2][nc] > dist[r][c] + 1) {
                dist[nr2][nc] = dist[r][c] + 1;
                q.push({nr2, nc});
                if (dist[nr2][nc] > maxDist) maxDist = dist[nr2][nc];
            }
        }
    }

    // ---------- Assign sea province IDs ----------
    // We'll assign sequential IDs starting after land provinces.
    uint32_t seaIdCounter = numLandProvinces + 1;
    std::vector<std::vector<uint32_t>> seaCellId(rows, std::vector<uint32_t>(cols, 0));
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!cellIsLand[r][c]) {
                seaCellId[r][c] = seaIdCounter++;
            }
        }
    }
    uint32_t numSeaProvinces = seaIdCounter - (numLandProvinces + 1);
    std::cout << "🌊 Number of sea provinces: " << numSeaProvinces << "\n";

    // ---------- Write land provinces.txt ----------
    std::cout << "📝 Writing provinces.txt...\n";
    std::ofstream ltxt(outLandTxt);
    if (!ltxt) {
        std::cerr << "❌ Could not create " << outLandTxt << "\n";
        return 1;
    }
    for (uint32_t id = 1; id <= numLandProvinces; ++id) {
        auto col = colorFromID(id);
        std::string name = "Province_" + std::to_string(id);
        ltxt << id << ";" << name << ";" << (int)col[0] << " " << (int)col[1] << " " << (int)col[2] << "\n";
        if (id % 100000 == 0) std::cout << "  wrote " << id << " entries\r" << std::flush;
    }
    ltxt.close();
    std::cout << "\n";

    // ---------- Write sea provinces.txt ----------
    std::cout << "📝 Writing seaprovinces.txt...\n";
    std::ofstream stxt(outSeaTxt);
    if (!stxt) {
        std::cerr << "❌ Could not create " << outSeaTxt << "\n";
        return 1;
    }
    uint32_t seaId = numLandProvinces + 1;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!cellIsLand[r][c]) {
                int d = dist[r][c];
                // Level based on distance: we can map distance to level (0..maxLevel)
                int level = (maxDist > 0) ? static_cast<int>(std::log2(d+1)) : 0; // example
                auto col = colorFromDistance(level, maxDist);
                std::string name = "Sea_Depth_" + std::to_string(level) + "_" + std::to_string(seaId);
                stxt << seaId << ";" << name << ";" << (int)col[0] << " " << (int)col[1] << " " << (int)col[2] << "\n";
                seaId++;
                if (seaId % 100000 == 0) std::cout << "  wrote " << (seaId - numLandProvinces - 1) << " entries\r" << std::flush;
            }
        }
    }
    stxt.close();
    std::cout << "\n";

    // ---------- Second pass: write province.bin, provincemap.tif, seaprovinces.tif ----------
    std::ofstream foutBin(outBin, std::ios::binary);
    if (!foutBin) {
        std::cerr << "❌ Could not create " << outBin << "\n";
        return 1;
    }

    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    char** tifOpts = nullptr;
    tifOpts = CSLSetNameValue(tifOpts, "COMPRESS", "LZW");
    tifOpts = CSLSetNameValue(tifOpts, "PREDICTOR", "2");
    tifOpts = CSLSetNameValue(tifOpts, "TILED", "YES");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKXSIZE", "512");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKYSIZE", "512");

    // Combined map (land + sea)
    GDALDataset* combDS = drv->Create(outCombinedTif.c_str(), width, height, 3, GDT_Byte, tifOpts);
    if (!combDS) {
        std::cerr << "❌ Could not create " << outCombinedTif << "\n";
        return 1;
    }
    double geoTransform[6];
    srcDS->GetGeoTransform(geoTransform);
    combDS->SetGeoTransform(geoTransform);
    combDS->SetProjection(srcDS->GetProjectionRef());

    // Sea‑only map (land = black)
    GDALDataset* seaDS = drv->Create(outSeaTif.c_str(), width, height, 3, GDT_Byte, tifOpts);
    if (!seaDS) {
        std::cerr << "❌ Could not create " << outSeaTif << "\n";
        return 1;
    }
    seaDS->SetGeoTransform(geoTransform);
    seaDS->SetProjection(srcDS->GetProjectionRef());

    std::vector<uint32_t> rowBin(width);
    std::vector<uint8_t> rowR(width), rowG(width), rowB(width);
    std::vector<uint8_t> rowSeaR(width), rowSeaG(width), rowSeaB(width);

    std::cout << "✍️  Second pass: writing province.bin, provincemap.tif, seaprovinces.tif...\n";

    for (int y = 0; y < height; ++y) {
        band->RasterIO(GF_Read, 0, y, width, 1, rowIn.data(), width, 1, GDT_Byte, 0, 0);
        for (int x = 0; x < width; ++x) {
            uint32_t provId = 0;
            // Determine hex cell
            Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
            int col = h.q + (h.r - (h.r & 1)) / 2;
            int row = h.r;
            if (col < 0) col = 0;
            if (col >= cols) col = cols - 1;
            if (row < 0) row = 0;
            if (row >= rows) row = rows - 1;

            bool isLand = cellIsLand[row][col];
            if (isLand) {
                provId = cellId[row][col];
                auto c = colorFromID(provId);
                rowR[x] = c[0]; rowG[x] = c[1]; rowB[x] = c[2];
                // Sea‑only: land = black
                rowSeaR[x] = 0; rowSeaG[x] = 0; rowSeaB[x] = 0;
            } else {
                provId = seaCellId[row][col];
                int d = dist[row][col];
                int level = (maxDist > 0) ? static_cast<int>(std::log2(d+1)) : 0;
                auto c = colorFromDistance(level, maxDist);
                rowR[x] = c[0]; rowG[x] = c[1]; rowB[x] = c[2];
                // Sea‑only: use same colours
                rowSeaR[x] = c[0]; rowSeaG[x] = c[1]; rowSeaB[x] = c[2];
            }
            rowBin[x] = provId;
        }

        foutBin.write(reinterpret_cast<const char*>(rowBin.data()), width * sizeof(uint32_t));
        combDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowR.data(), width, 1, GDT_Byte, 0, 0);
        combDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, rowG.data(), width, 1, GDT_Byte, 0, 0);
        combDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, rowB.data(), width, 1, GDT_Byte, 0, 0);

        seaDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowSeaR.data(), width, 1, GDT_Byte, 0, 0);
        seaDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, rowSeaG.data(), width, 1, GDT_Byte, 0, 0);
        seaDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, rowSeaB.data(), width, 1, GDT_Byte, 0, 0);

        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    foutBin.close();
    GDALClose(combDS);
    GDALClose(seaDS);
    GDALClose(srcDS);

    std::cout << "✅ Done!\n";
    std::cout << "  province.bin     : " << outBin << " (size ~" << (width*height*4/1024/1024) << " MB)\n";
    std::cout << "  provinces.txt    : " << outLandTxt << "\n";
    std::cout << "  seaprovinces.txt : " << outSeaTxt << "\n";
    std::cout << "  provincemap.tif  : " << outCombinedTif << "\n";
    std::cout << "  seaprovinces.tif : " << outSeaTif << "\n";

    return 0;
}