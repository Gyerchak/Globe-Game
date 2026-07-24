// GenerateProvinces.cpp
// Reads the 8-bit heightmap and creates a hex‑province map.
// Compile: g++ -std=c++20 -O3 GenerateProvinces.cpp -o exe/GenerateProvinces -lgdal
// Run: ./exe/GenerateProvinces

#include <gdal_priv.h>
#include <gdal_utils.h>
#include <cpl_conv.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <random>
#include <string>
#include <sstream>
#include <map>
#include <set>

namespace fs = std::filesystem;

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 32768;

// Hexagon lattice parameters
constexpr double HEX_RADIUS = 110.0;        // pixels (adjust for more/fewer provinces)
constexpr double DX = 1.5 * HEX_RADIUS;     // horizontal spacing
constexpr double DY = std::sqrt(3.0) * HEX_RADIUS; // vertical spacing

// -----------------------------------------------------------------------------
// 1. City names (hard‑coded sample – you can extend this list)
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
// 2. Helper: generate a random RGB colour
// -----------------------------------------------------------------------------
static std::array<uint8_t, 3> randomColor(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 255);
    return { static_cast<uint8_t>(dist(rng)),
             static_cast<uint8_t>(dist(rng)),
             static_cast<uint8_t>(dist(rng)) };
}

// -----------------------------------------------------------------------------
// 3. Main
// -----------------------------------------------------------------------------
int main() {
    GDALAllRegister();

    // Paths
    fs::path inHeight = fs::current_path() / "input" / "heightmap_8bit.tif";
    fs::path outBin = fs::current_path() / "output" / "province.bin";
    fs::path outTxt = fs::current_path() / "output" / "provinces.txt";
    fs::path outPNG = fs::current_path() / "output" / "provincemap.png";

    if (!fs::exists(inHeight)) {
        std::cerr << "❌ Input heightmap not found: " << inHeight << "\n";
        return 1;
    }

    fs::create_directories(outBin.parent_path());

    // Open source heightmap
    GDALDataset* srcDS = (GDALDataset*)GDALOpen(inHeight.c_str(), GA_ReadOnly);
    if (!srcDS) {
        std::cerr << "❌ Could not open " << inHeight << "\n";
        return 1;
    }

    int width = srcDS->GetRasterXSize();
    int height = srcDS->GetRasterYSize();
    if (width != WIDTH || height != HEIGHT) {
        std::cerr << "⚠️ Size mismatch: expected " << WIDTH << "x" << HEIGHT
                  << ", got " << width << "x" << height << "\n";
    }

    GDALRasterBand* band = srcDS->GetRasterBand(1);

    // ---------- Step 1: Determine hex grid size ----------
    // Compute number of columns and rows of hex cells.
    int cols = static_cast<int>(std::ceil(width / DX)) + 2;
    int rows = static_cast<int>(std::ceil(height / DY)) + 2;

    // Offsets: the first cell centre is at (DX/2, DY/2) so that the grid is centred.
    double xOffset = 0.5 * DX;
    double yOffset = 0.5 * DY;

    std::cout << "🗺️  Hex grid: " << cols << " columns x " << rows << " rows = "
              << (cols * rows) << " cells\n";

    // ---------- Step 2: Prepare output buffers ----------
    // We'll store province ID for each cell (initially 0 = water).
    // We'll use a 2D vector indexed by (row, col) for convenience.
    std::vector<std::vector<int>> cellId(rows, std::vector<int>(cols, 0));
    // Also a flag if cell contains any land.
    std::vector<std::vector<bool>> cellHasLand(rows, std::vector<bool>(cols, false));

    // We'll also write province.bin as we go, but we need to know the ID for each pixel.
    // Since we'll assign IDs after we know which cells are land, we need to store temporary IDs.
    // We'll use a 16-bit array for the whole image? That's 2.1e9 * 2 = 4.3 GB, too much.
    // Better approach: first pass: determine which cells are land, assign IDs to cells, then second pass: write province.bin with those IDs.
    // So we'll do two passes over the heightmap.
    // First pass: for each pixel, find its hex cell, mark cellHasLand, and maybe store a mapping pixel->cell index? That would be huge.
    // Instead, we can compute the cell ID for each pixel on the fly in the second pass, using the precomputed cell->province mapping.
    // So we need a fast way to get cell index from pixel coordinates.
    // We'll implement a function that given (x,y) returns (row, col) of the nearest hex center.

    // ---------- Step 3: First pass – mark land cells ----------
    std::cout << "🔎 First pass: scanning for land cells...\n";
    std::vector<uint8_t> rowIn(width);

    // Lambda: given x,y, compute hex cell (row, col)
    auto getHexCell = [&](int x, int y) -> std::pair<int, int> {
        // Convert to fractional hex coordinates.
        // We use the standard hexagonal grid with pointy-top orientation.
        // Coordinate system: q = ( (x - xOffset) * 2/3 ) / DX, r = ( (y - yOffset) / DY ) - 0.5 * (q)
        // Actually simpler: find nearest centre using distance.
        // We'll just use the formula for hexagonal lattice.
        // For pointy-top hexagons, the centres are at:
        //   cx = xOffset + i*DX
        //   cy = yOffset + j*DY  (if i even? offset)
        // But we have a staggered grid: every other row is shifted by DX/2.
        // So we compute the row and column index:
        double fx = (x - xOffset) / DX;
        double fy = (y - yOffset) / DY;
        // Row index: fy
        int j = static_cast<int>(std::floor(fy + 0.5));
        // Column index depends on row parity:
        double offsetX = (j % 2 == 0) ? 0.0 : 0.5 * DX;
        double xRel = x - (xOffset + offsetX) - j * 0.0; // actually formula: column = (x - xOffset - (j%2)*DX/2) / DX
        int i = static_cast<int>(std::floor((x - xOffset - (j % 2) * 0.5 * DX) / DX + 0.5));
        // Clamp to grid
        if (i < 0) i = 0;
        if (i >= cols) i = cols - 1;
        if (j < 0) j = 0;
        if (j >= rows) j = rows - 1;
        return {j, i};
    };

    for (int y = 0; y < height; ++y) {
        band->RasterIO(GF_Read, 0, y, width, 1, rowIn.data(), width, 1, GDT_Byte, 0, 0);
        for (int x = 0; x < width; ++x) {
            if (rowIn[x] != 0) { // land
                auto [row, col] = getHexCell(x, y);
                cellHasLand[row][col] = true;
            }
        }
        if (y % 10000 == 0) std::cout << "  row " << y << " / " << height << "\n";
    }

    // ---------- Step 4: Assign province IDs to land cells in raster order ----------
    // Collect all land cells (row, col) and sort by (row, col) to get left-to-right, top-to-bottom order.
    std::vector<std::pair<int, int>> landCells;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cellHasLand[r][c]) {
                landCells.push_back({r, c});
            }
        }
    }

    int numProvinces = static_cast<int>(landCells.size());
    std::cout << "🏛️  Number of provinces (land hexagons): " << numProvinces << "\n";

    // Assign ID (starting from 1)
    for (int idx = 0; idx < numProvinces; ++idx) {
        auto [r, c] = landCells[idx];
        cellId[r][c] = idx + 1; // 1-based
    }

    // ---------- Step 5: Generate provinces.txt with colours and names ----------
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> nameDist(0, CITY_NAMES.size() - 1);

    std::ofstream txt(outTxt);
    if (!txt) {
        std::cerr << "❌ Could not create " << outTxt << "\n";
        return 1;
    }

    // For each province (ID = 1..numProvinces), assign name and colour.
    std::vector<std::array<uint8_t, 3>> palette(numProvinces + 1); // 1-indexed
    std::vector<std::string> provinceNames(numProvinces + 1);

    // We'll use a set to avoid duplicate names if possible.
    std::set<std::string> usedNames;

    for (int id = 1; id <= numProvinces; ++id) {
        // Random colour (ensuring not too dark)
        auto col = randomColor(rng);
        // Ensure brightness > 30 to avoid near-black
        while ((col[0] + col[1] + col[2]) < 60) {
            col = randomColor(rng);
        }
        palette[id] = col;

        // Name: try to pick a city name, fallback to Wasteland_XXX
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

    // ---------- Step 6: Second pass – write province.bin and also create provincemap.png ----------
    // We'll create a GDAL dataset for province.bin as 16-bit, and for provincemap.png as 3-band Byte.
    // But we can write province.bin as raw binary (2 bytes per pixel) and also create PNG.

    // Open province.bin for writing
    std::ofstream foutBin(outBin, std::ios::binary);
    if (!foutBin) {
        std::cerr << "❌ Could not create " << outBin << "\n";
        return 1;
    }

    // Create PNG dataset using GDAL
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    char** pngOpts = nullptr;
    pngOpts = CSLSetNameValue(pngOpts, "COMPRESS", "DEFLATE");
    pngOpts = CSLSetNameValue(pngOpts, "TILED", "YES");
    GDALDataset* pngDS = drv->Create(outPNG.c_str(), width, height, 3, GDT_Byte, pngOpts);
    if (!pngDS) {
        std::cerr << "❌ Could not create " << outPNG << "\n";
        return 1;
    }
    // Copy geotransform and projection from source
    double geoTransform[6];
    srcDS->GetGeoTransform(geoTransform);
    pngDS->SetGeoTransform(geoTransform);
    pngDS->SetProjection(srcDS->GetProjectionRef());

    // Buffers for each row
    std::vector<uint16_t> rowBin(width);
    std::vector<uint8_t> rowR(width), rowG(width), rowB(width);

    std::cout << "✍️  Second pass: writing province.bin and provincemap.png...\n";

    for (int y = 0; y < height; ++y) {
        // Read heightmap row again
        band->RasterIO(GF_Read, 0, y, width, 1, rowIn.data(), width, 1, GDT_Byte, 0, 0);

        for (int x = 0; x < width; ++x) {
            uint16_t provId = 0;
            if (rowIn[x] != 0) {
                auto [r, c] = getHexCell(x, y);
                provId = static_cast<uint16_t>(cellId[r][c]);
            }
            rowBin[x] = provId;
            // Fill RGB
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

        // Write binary
        foutBin.write(reinterpret_cast<const char*>(rowBin.data()), width * sizeof(uint16_t));

        // Write PNG bands
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