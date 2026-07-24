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
#include <set>

namespace fs = std::filesystem;

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 32768;

// Hexagon parameters (pointy‑top)
constexpr double HEX_RADIUS = 47.0;        // pixels (centre to corner)
constexpr double DX = std::sqrt(3.0) * HEX_RADIUS;   // horizontal spacing
constexpr double DY = 1.5 * HEX_RADIUS;              // vertical spacing

// -----------------------------------------------------------------------------
// City names (extend as needed)
// -----------------------------------------------------------------------------
static const std::vector<std::string> CITY_NAMES = {
    "London", "Paris", "Berlin", "Madrid", "Rome", "Athens", "Moscow", "Beijing",
    "Tokyo", "Seoul", "Bangkok", "Mumbai", "Delhi", "Cairo", "Nairobi", "CapeTown",
    "Sydney", "Melbourne", "Auckland", "Wellington", "NewYork", "LosAngeles",
    "Chicago", "Houston", "Toronto", "Vancouver", "MexicoCity", "Brasilia",
    "BuenosAires", "Santiago", "Lima", "Bogota", "Johannesburg", "Kinshasa",
    "Lagos", "Accra", "Dakar", "Marrakech", "Tunis", "Algiers", "Tripoli",
    "Damascus", "Beirut", "Amman", "Jerusalem", "Riyadh", "Dubai", "Tehran",
    "Baghdad", "Ankara", "Kiev", "Warsaw", "Prague", "Vienna", "Budapest",
    "Bucharest", "Sofia", "Belgrade", "Zagreb", "Ljubljana", "Bratislava",
    "Helsinki", "Stockholm", "Oslo", "Copenhagen", "Dublin", "Edinburgh",
    "Manchester", "Birmingham", "Leeds", "Glasgow", "Bristol", "Liverpool",
    "Sheffield", "Nottingham", "Leicester", "Coventry", "Cardiff", "Swansea",
    "Belfast", "Derry", "Newcastle", "Sunderland", "Stoke", "Wolverhampton",
    "Blackpool", "Plymouth", "Exeter", "Norwich", "Ipswich", "Brighton",
    "Southampton", "Portsmouth", "Reading", "Oxford", "Cambridge", "Aberdeen",
    "Dundee", "Inverness", "Perth", "Stirling", "Edmonton", "Calgary",
    "Winnipeg", "Ottawa", "Montreal", "Quebec", "Halifax", "StJohns",
    "Anchorage", "Fairbanks", "Juneau", "Honolulu", "Hilo", "Kailua",
    "Miami", "Orlando", "Tampa", "Jacksonville", "Atlanta", "Charlotte",
    "Raleigh", "Nashville", "Memphis", "NewOrleans", "Dallas", "Houston",
    "SanAntonio", "Austin", "Phoenix", "SanDiego", "SanFrancisco", "Oakland",
    "SanJose", "Seattle", "Portland", "Denver", "SaltLakeCity", "Albuquerque",
    "Tucson", "ElPaso", "OklahomaCity", "Tulsa", "KansasCity", "StLouis",
    "Indianapolis", "Cincinnati", "Cleveland", "Columbus", "Detroit",
    "Milwaukee", "Minneapolis", "StPaul", "Pittsburgh", "Philadelphia",
    "Baltimore", "Washington", "Boston", "Providence", "Hartford"
};

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
// Unique random colour generator
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
    } while (used.find(col) != used.end() || (col[0] + col[1] + col[2]) < 60); // avoid too dark
    used.insert(col);
    return col;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    GDALAllRegister();

    fs::path inHeight = fs::current_path() / "input" / "heightmap_8bit.tif";
    fs::path outBin = fs::current_path() / "output" / "province.bin";
    fs::path outTxt = fs::current_path() / "output" / "provinces.txt";
    fs::path outPNG = fs::current_path() / "output" / "provincemap.png";

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
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellHasLand[r][c]) landCells.push_back({r, c});
        }
    }

    int numProvinces = static_cast<int>(landCells.size());
    std::cout << "🏛️  Number of provinces (land hexagons): " << numProvinces << "\n";

    // Assign IDs
    std::vector<std::vector<int>> cellId(rows, std::vector<int>(cols, 0));
    for (int idx = 0; idx < numProvinces; ++idx) {
        auto [r, c] = landCells[idx];
        cellId[r][c] = idx + 1;
    }

    // ---------- Generate unique colours and names ----------
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> nameDist(0, CITY_NAMES.size() - 1);
    std::set<Color> usedColors;
    std::set<std::string> usedNames;

    std::vector<Color> palette(numProvinces + 1);
    std::vector<std::string> provinceNames(numProvinces + 1);

    std::ofstream txt(outTxt);
    if (!txt) {
        std::cerr << "❌ Could not create " << outTxt << "\n";
        return 1;
    }

    for (int id = 1; id <= numProvinces; ++id) {
        Color col = uniqueRandomColor(rng, usedColors);
        palette[id] = col;

        std::string name;
        int attempts = 0;
        do {
            int idx = nameDist(rng);
            name = CITY_NAMES[idx];
            attempts++;
        } while (usedNames.find(name) != usedNames.end() && attempts < 100);
        if (attempts >= 100) {
            name = "Wasteland_" + std::to_string(id);
        }
        usedNames.insert(name);
        provinceNames[id] = name;

        txt << id << ";" << name << ";" << (int)col[0] << " " << (int)col[1] << " " << (int)col[2] << "\n";
    }
    txt.close();

    // ---------- Second pass: write province.bin and provincemap.png ----------
    std::ofstream foutBin(outBin, std::ios::binary);
    if (!foutBin) {
        std::cerr << "❌ Could not create " << outBin << "\n";
        return 1;
    }

    // Create PNG (GeoTIFF) output
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    char** pngOpts = nullptr;
    pngOpts = CSLSetNameValue(pngOpts, "COMPRESS", "DEFLATE");
    pngOpts = CSLSetNameValue(pngOpts, "TILED", "YES");
    GDALDataset* pngDS = drv->Create(outPNG.c_str(), width, height, 3, GDT_Byte, pngOpts);
    if (!pngDS) {
        std::cerr << "❌ Could not create " << outPNG << "\n";
        return 1;
    }
    double geoTransform[6];
    srcDS->GetGeoTransform(geoTransform);
    pngDS->SetGeoTransform(geoTransform);
    pngDS->SetProjection(srcDS->GetProjectionRef());

    std::vector<uint16_t> rowBin(width);
    std::vector<uint8_t> rowR(width), rowG(width), rowB(width);

    std::cout << "✍️  Second pass: writing province.bin and provincemap.png...\n";

    for (int y = 0; y < height; ++y) {
        band->RasterIO(GF_Read, 0, y, width, 1, rowIn.data(), width, 1, GDT_Byte, 0, 0);
        for (int x = 0; x < width; ++x) {
            uint16_t provId = 0;
            if (rowIn[x] != 0) {
                Hex h = pixelToHex(static_cast<double>(x), static_cast<double>(y), xOff, yOff);
                int col = h.q + (h.r - (h.r & 1)) / 2;
                int row = h.r;
                if (col < 0) col = 0;
                if (col >= cols) col = cols - 1;
                if (row < 0) row = 0;
                if (row >= rows) row = rows - 1;
                provId = static_cast<uint16_t>(cellId[row][col]);
            }
            rowBin[x] = provId;
            if (provId != 0) {
                auto& col = palette[provId];
                rowR[x] = col[0];
                rowG[x] = col[1];
                rowB[x] = col[2];
            } else {
                rowR[x] = 0;
                rowG[x] = 0;
                rowB[x] = 0;
            }
        }

        foutBin.write(reinterpret_cast<const char*>(rowBin.data()), width * sizeof(uint16_t));
        pngDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1, rowR.data(), width, 1, GDT_Byte, 0, 0);
        pngDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1, rowG.data(), width, 1, GDT_Byte, 0, 0);
        pngDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1, rowB.data(), width, 1, GDT_Byte, 0, 0);

        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    foutBin.close();
    GDALClose(pngDS);
    GDALClose(srcDS);

    std::cout << "✅ Done!\n";
    std::cout << "  province.bin     : " << outBin << "\n";
    std::cout << "  provinces.txt    : " << outTxt << "\n";
    std::cout << "  provincemap.png  : " << outPNG << "\n";

    return 0;
}