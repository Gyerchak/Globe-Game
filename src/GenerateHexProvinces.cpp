// GenerateHexProvinces.cpp – fast version (no unique colour set)
// Compile: g++ -std=c++20 -O3 GenerateHexProvinces.cpp -o exe/GenerateHexProvinces -lgdal

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

namespace fs = std::filesystem;

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 32768;
constexpr double HEX_RADIUS = 6.0;
constexpr double DX = std::sqrt(3.0) * HEX_RADIUS;
constexpr double DY = 1.5 * HEX_RADIUS;
constexpr uint8_t COASTAL_HEIGHT = 10;

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

using Color = std::array<uint8_t, 3>;

// Deterministic colour from ID
static inline Color colorFromID(uint32_t id) {
    uint32_t hash = id * 2654435761u;
    uint32_t rgb = hash & 0xFFFFFF;
    return {
        static_cast<uint8_t>((rgb >> 16) & 0xFF),
        static_cast<uint8_t>((rgb >> 8) & 0xFF),
        static_cast<uint8_t>(rgb & 0xFF)
    };
}

// Build palette (same as before)
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

int main() {
    GDALAllRegister();
    std::ios::sync_with_stdio(false);

    fs::path inPath = fs::current_path() / "input" / "heightmap_8bit.tif";
    fs::path outHex = fs::current_path() / "output" / "hexheightmap.tif";
    fs::path outHexCol = fs::current_path() / "output" / "hexheightmap_color.tif";
    fs::path outBin = fs::current_path() / "output" / "province.bin";
    fs::path outLandTxt = fs::current_path() / "output" / "landprovinces.txt";
    fs::path outSeaTxt = fs::current_path() / "output" / "seaprovinces.txt";
    fs::path outLandMap = fs::current_path() / "output" / "landprovincemap.tif";
    fs::path outSeaMap = fs::current_path() / "output" / "seaprovincemap.tif";

    if (!fs::exists(inPath)) { std::cerr << "❌ Input not found\n"; return 1; }
    fs::create_directories(outHex.parent_path());

    auto palette = buildPalette();

    GDALDataset* srcDS = (GDALDataset*)GDALOpen(inPath.c_str(), GA_ReadOnly);
    if (!srcDS) { std::cerr << "❌ Could not open input\n"; return 1; }

    int width = srcDS->GetRasterXSize();
    int height = srcDS->GetRasterYSize();
    GDALRasterBand* band = srcDS->GetRasterBand(1);

    double xOff = 0.0, yOff = 0.0;
    int cols = static_cast<int>(std::ceil(width / DX)) + 2;
    int rows = static_cast<int>(std::ceil(height / DY)) + 2;
    std::cout << "🗺️  Hex grid (radius 6): " << cols << " x " << rows << " = " << (cols * rows) << " cells\n";

    std::vector<std::vector<uint64_t>> sumLand(rows, std::vector<uint64_t>(cols, 0));
    std::vector<std::vector<uint32_t>> countLand(rows, std::vector<uint32_t>(cols, 0));
    std::vector<std::vector<uint32_t>> countSea(rows, std::vector<uint32_t>(cols, 0));
    std::vector<uint8_t> rowIn(width);

    std::cout << "🔎 Phase 1: Scanning...\n";
    for (int y = 0; y < height; ++y) {
        band->RasterIO(GF_Read, 0, y, width, 1, rowIn.data(), width, 1, GDT_Byte, 0, 0);
        for (int x = 0; x < width; ++x) {
            uint8_t val = rowIn[x];
            Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
            int col = h.q + (h.r - (h.r & 1)) / 2;
            int row = h.r;
            if (col < 0) col = 0; if (col >= cols) col = cols-1;
            if (row < 0) row = 0; if (row >= rows) row = rows-1;
            if (val > 0) { sumLand[row][col] += val; countLand[row][col]++; }
            else { countSea[row][col]++; }
        }
        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    std::vector<std::vector<bool>> cellIsLand(rows, std::vector<bool>(cols, false));
    std::vector<std::vector<uint8_t>> cellHeight(rows, std::vector<uint8_t>(cols, 0));
    std::vector<std::vector<bool>> cellIsCoastal(rows, std::vector<bool>(cols, false));

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (countLand[r][c] > 0 && countSea[r][c] == 0) {
                cellIsLand[r][c] = true;
                uint8_t avg = static_cast<uint8_t>(sumLand[r][c] / countLand[r][c]);
                cellHeight[r][c] = avg;
            } else if (countLand[r][c] == 0 && countSea[r][c] > 0) {
                cellIsLand[r][c] = false;
                cellHeight[r][c] = 0;
            } else if (countLand[r][c] > 0 && countSea[r][c] > 0) {
                cellIsLand[r][c] = true;
                cellIsCoastal[r][c] = true;
                cellHeight[r][c] = COASTAL_HEIGHT;
            } else {
                cellIsLand[r][c] = false;
                cellHeight[r][c] = 0;
            }
        }
    }

    // Write hexheightmap
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    char** tifOpts = nullptr;
    tifOpts = CSLSetNameValue(tifOpts, "COMPRESS", "LZW");
    tifOpts = CSLSetNameValue(tifOpts, "PREDICTOR", "2");
    tifOpts = CSLSetNameValue(tifOpts, "TILED", "YES");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKXSIZE", "512");
    tifOpts = CSLSetNameValue(tifOpts, "BLOCKYSIZE", "512");

    GDALDataset* hexDS = drv->Create(outHex.c_str(), width, height, 1, GDT_Byte, tifOpts);
    GDALDataset* hexColDS = drv->Create(outHexCol.c_str(), width, height, 3, GDT_Byte, tifOpts);
    double geoTransform[6]; srcDS->GetGeoTransform(geoTransform);
    hexDS->SetGeoTransform(geoTransform); hexDS->SetProjection(srcDS->GetProjectionRef());
    hexColDS->SetGeoTransform(geoTransform); hexColDS->SetProjection(srcDS->GetProjectionRef());

    std::vector<uint8_t> rowHt(width), rowR(width), rowG(width), rowB(width);
    std::cout << "✍️  Writing hexheightmap...\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
            int col = h.q + (h.r - (h.r & 1)) / 2; int row = h.r;
            if (col < 0) col = 0; if (col >= cols) col = cols-1;
            if (row < 0) row = 0; if (row >= rows) row = rows-1;
            uint8_t ht = cellHeight[row][col];
            rowHt[x] = ht;
            auto c = palette[ht];
            rowR[x] = c[0]; rowG[x] = c[1]; rowB[x] = c[2];
        }
        hexDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowHt.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowR.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, rowG.data(), width, 1, GDT_Byte, 0, 0);
        hexColDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, rowB.data(), width, 1, GDT_Byte, 0, 0);
        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }
    GDALClose(hexDS); GDALClose(hexColDS); GDALClose(srcDS);

    fs::copy_file(outHex, fs::current_path() / "input" / "hexheightmap.tif", fs::copy_options::overwrite_existing);
    std::cout << "📁 Copied hexheightmap.tif to input/\n";

    // ---------- Fast province generation (no set) ----------
    std::vector<std::pair<int,int>> landCells, seaCells;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            if (cellIsLand[r][c] || cellIsCoastal[r][c]) landCells.push_back({r,c});
            else seaCells.push_back({r,c});

    uint32_t numLand = landCells.size();
    uint32_t numSea = seaCells.size();
    std::cout << "🏛️  Land: " << numLand << ", Sea: " << numSea << "\n";

    // Assign IDs
    std::vector<std::vector<uint32_t>> cellId(rows, std::vector<uint32_t>(cols, 0));
    uint32_t id = 1;
    for (auto &p : landCells) cellId[p.first][p.second] = id++;
    for (auto &p : seaCells)  cellId[p.first][p.second] = id++;

    // Write text files (fast, no endl flush)
    std::cout << "📝 Writing landprovinces.txt...\n";
    std::ofstream ltxt(outLandTxt);
    if (!ltxt) return 1;
    for (uint32_t i = 1; i <= numLand; ++i) {
        auto c = colorFromID(i);
        ltxt << i << ";Province_Land_" << i << ";" << (int)c[0] << " " << (int)c[1] << " " << (int)c[2] << "\n";
        if (i % 100000 == 0) std::cout << "  wrote " << i << "\r" << std::flush;
    }
    ltxt.close(); std::cout << "\n";

    std::cout << "📝 Writing seaprovinces.txt...\n";
    std::ofstream stxt(outSeaTxt);
    for (uint32_t i = 1; i <= numSea; ++i) {
        auto c = colorFromID(numLand + i); // unique from land
        uint32_t globalId = numLand + i;
        stxt << globalId << ";Province_Sea_" << i << ";" << (int)c[0] << " " << (int)c[1] << " " << (int)c[2] << "\n";
        if (i % 100000 == 0) std::cout << "  wrote " << i << "\r" << std::flush;
    }
    stxt.close(); std::cout << "\n";

    // province.bin
    std::ofstream foutBin(outBin, std::ios::binary);
    if (!foutBin) return 1;

    GDALDataset* landMapDS = drv->Create(outLandMap.c_str(), width, height, 3, GDT_Byte, tifOpts);
    GDALDataset* seaMapDS = drv->Create(outSeaMap.c_str(), width, height, 3, GDT_Byte, tifOpts);
    landMapDS->SetGeoTransform(geoTransform); landMapDS->SetProjection("WGS84");
    seaMapDS->SetGeoTransform(geoTransform); seaMapDS->SetProjection("WGS84");

    std::vector<uint32_t> rowBin32(width);
    std::vector<uint8_t> lR(width), lG(width), lB(width), sR(width), sG(width), sB(width);

    std::cout << "✍️  Writing province.bin and maps...\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
            int col = h.q + (h.r - (h.r & 1)) / 2; int row = h.r;
            if (col < 0) col = 0; if (col >= cols) col = cols-1;
            if (row < 0) row = 0; if (row >= rows) row = rows-1;
            uint32_t provId = cellId[row][col];
            rowBin32[x] = provId;
            bool isLand = cellIsLand[row][col] || cellIsCoastal[row][col];
            if (isLand) {
                auto c = colorFromID(provId);
                lR[x]=c[0]; lG[x]=c[1]; lB[x]=c[2];
                sR[x]=sG[x]=sB[x]=0;
            } else {
                uint32_t seaIdx = provId - numLand;
                auto c = colorFromID(numLand + seaIdx);
                sR[x]=c[0]; sG[x]=c[1]; sB[x]=c[2];
                lR[x]=lG[x]=lB[x]=0;
            }
        }
        foutBin.write((char*)rowBin32.data(), width*4);
        landMapDS->GetRasterBand(1)->RasterIO(GF_Write,0,y,width,1,lR.data(),width,1,GDT_Byte,0,0);
        landMapDS->GetRasterBand(2)->RasterIO(GF_Write,0,y,width,1,lG.data(),width,1,GDT_Byte,0,0);
        landMapDS->GetRasterBand(3)->RasterIO(GF_Write,0,y,width,1,lB.data(),width,1,GDT_Byte,0,0);
        seaMapDS->GetRasterBand(1)->RasterIO(GF_Write,0,y,width,1,sR.data(),width,1,GDT_Byte,0,0);
        seaMapDS->GetRasterBand(2)->RasterIO(GF_Write,0,y,width,1,sG.data(),width,1,GDT_Byte,0,0);
        seaMapDS->GetRasterBand(3)->RasterIO(GF_Write,0,y,width,1,sB.data(),width,1,GDT_Byte,0,0);
        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }
    foutBin.close(); GDALClose(landMapDS); GDALClose(seaMapDS);

    std::cout << "✅ Done!\n";
    return 0;
}