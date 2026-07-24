// CompressHeightMap.cpp
// Reads the 32‑bit global mosaic and produces:
//   - output/heightmap_8bit.tif   (Byte, 1 band, 0‑255)
//   - output/heightmap_color.tif  (Byte, 3 band RGB)
// Compile: g++ -std=c++20 -O3 CompressHeightMap.cpp -o CompressHeightMap -lgdal
// Run: ./CompressHeightMap

#include <gdal_priv.h>
#include <gdal_utils.h>
#include <cpl_conv.h>

#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <array>
#include <cmath>

namespace fs = std::filesystem;

constexpr int WIDTH = 65536;
constexpr int HEIGHT = 32768;

// -----------------------------------------------------------------------------
// Progressive band mapping – as provided in the original code
// -----------------------------------------------------------------------------
constexpr double neg_zero = -1.0 / 32768.0;
constexpr double pos_zero = 1.0 / 32768.0;

constexpr std::array<double, 10> subsea_upper = {
    -400.0, -300.0, -250.0, -200.0, -150.0,
    -100.0, -50.0, -20.0, -7.8, neg_zero};

    constexpr std::array<double, 245> positive_upper = {
        7.8, 15.5, 25.8, 36.2, 46.5, 54.3, 62.0, 69.8, 77.5, 85.3,
        93.0, 100.8, 108.5, 113.7, 118.8, 124.0, 129.2, 134.3, 139.5, 144.7,
        149.8, 155.0, 160.2, 165.3, 170.5, 174.9, 179.4, 183.8, 188.2, 192.6,
        197.1, 201.5, 205.9, 210.4, 214.8, 219.2, 223.6, 228.1, 232.5, 236.9,
        241.4, 245.8, 250.2, 254.6, 259.1, 263.5, 267.9, 272.4, 276.8, 281.2,
        285.6, 290.1, 294.5, 298.9, 303.4, 307.8, 312.2, 316.6, 321.1, 325.5,
        331.7, 337.9, 344.1, 350.3, 356.5, 362.7, 368.9, 375.1, 381.3, 387.5,
        393.7, 399.9, 406.1, 412.3, 418.5, 426.3, 434.0, 441.8, 449.5, 459.8,
        470.2, 480.5, 490.8, 501.2, 511.5, 521.8, 532.2, 542.5, 552.8, 563.2,
        573.5, 589.0, 604.5, 620.0, 635.5, 651.0, 666.5, 682.0, 697.5, 713.0,
        728.5, 744.0, 759.5, 790.5, 806.0, 821.5, 852.5, 868.0, 883.5, 914.5,
        945.5, 961.0, 976.5, 1007.5, 1038.5, 1069.5, 1085.0, 1100.5, 1131.5, 1162.5,
        1193.5, 1224.5, 1255.5, 1271.0, 1286.5, 1317.5, 1348.5, 1379.5, 1410.5, 1441.5,
        1472.5, 1503.5, 1534.5, 1565.5, 1596.5, 1627.5, 1658.5, 1689.5, 1720.5, 1751.5,
        1782.5, 1813.5, 1844.5, 1875.5, 1906.5, 1937.5, 1968.5, 1999.5, 2030.5, 2061.5,
        2108.0, 2170.0, 2232.0, 2294.0, 2356.0, 2418.0, 2480.0, 2542.0, 2604.0, 2666.0,
        2728.0, 2790.0, 2852.0, 2914.0, 2976.0, 3038.0, 3100.0, 3162.0, 3224.0, 3286.0,
        3348.0, 3410.0, 3472.0, 3534.0, 3596.0, 3658.0, 3720.0, 3782.0, 3844.0, 3906.0,
        3968.0, 4030.0, 4092.0, 4154.0, 4216.0, 4278.0, 4340.0, 4402.0, 4464.0, 4526.0,
        4588.0, 4650.0, 4712.0, 4774.0, 4836.0, 4898.0, 4960.0, 5022.0, 5084.0, 5146.0,
        5208.0, 5270.0, 5332.0, 5394.0, 5456.0, 5518.0, 5580.0, 5642.0, 5704.0, 5766.0,
        5828.0, 5890.0, 5952.0, 6014.0, 6076.0, 6138.0, 6200.0, 6262.0, 6324.0, 6386.0,
        6448.0, 6510.0, 6572.0, 6634.0, 6696.0, 6758.0, 6820.0, 6882.0, 6944.0, 7006.0,
        7068.0, 7130.0, 7192.0, 7254.0, 7316.0, 7386.0, 7466.0, 7556.0, 7666.0, 7796.0,
        7946.0, 8126.0, 8336.0, 8576.0, 8846.0};

        inline uint8_t compress(float elevation) {
            if (std::abs(elevation) < 1e-6f) return 0;          // water (exact 0)
            if (elevation < 0.0f) {
                const auto it = std::lower_bound(subsea_upper.begin(), subsea_upper.end(), static_cast<double>(elevation));
                const auto idx = std::distance(subsea_upper.begin(), it);
                return static_cast<uint8_t>(idx + 1);           // 1‑10
            }
            if (elevation >= 8846.0f) return 255;                // max value
            const auto it = std::lower_bound(positive_upper.begin(), positive_upper.end(), static_cast<double>(elevation));
            const auto idx = std::distance(positive_upper.begin(), it);
            return static_cast<uint8_t>(idx + 11);               // 11‑255
        }

        // -----------------------------------------------------------------------------
        // Build a 256‑entry colour palette from the provided ramp
        // -----------------------------------------------------------------------------
        struct RGB { uint8_t r, g, b; };

        static std::array<RGB, 256> buildPalette() {
            // Known control points: (value, r, g, b)
            struct Point { int val; uint8_t r, g, b; };
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
            // Sort by value (already sorted)
            std::array<RGB, 256> pal{};
            for (size_t i = 0; i < pts.size(); ++i) {
                int startVal = pts[i].val;
                int endVal = (i + 1 < pts.size()) ? pts[i+1].val : 255;
                float steps = endVal - startVal;
                if (steps <= 0) continue;
                for (int v = startVal; v <= endVal; ++v) {
                    float t = (v - startVal) / steps;
                    uint8_t r = static_cast<uint8_t>(pts[i].r + t * (pts[i+1].r - pts[i].r));
                    uint8_t g = static_cast<uint8_t>(pts[i].g + t * (pts[i+1].g - pts[i].g));
                    uint8_t b = static_cast<uint8_t>(pts[i].b + t * (pts[i+1].b - pts[i].b));
                    pal[v] = {r, g, b};
                }
            }
            return pal;
        }

        // -----------------------------------------------------------------------------
        // Main
        // -----------------------------------------------------------------------------
        int main() {
            GDALAllRegister();

            // Paths
            fs::path inFile = fs::current_path() / "input" / "united" / "global_mosaic.tif";
            fs::path out8 = fs::current_path() / "output" / "heightmap_8bit.tif";
            fs::path outColor = fs::current_path() / "output" / "heightmap_color.tif";

            if (!fs::exists(inFile)) {
                std::cerr << "❌ Input file not found: " << inFile << "\n";
                return 1;
            }

            fs::create_directories(out8.parent_path());

            // Open source dataset
            GDALDataset* srcDS = (GDALDataset*)GDALOpen(inFile.c_str(), GA_ReadOnly);
            if (!srcDS) {
                std::cerr << "❌ Failed to open " << inFile << "\n";
                return 1;
            }

            int width = srcDS->GetRasterXSize();
            int height = srcDS->GetRasterYSize();
            if (width != WIDTH || height != HEIGHT) {
                std::cerr << "⚠️  Input size " << width << "x" << height
                << " does not match expected " << WIDTH << "x" << HEIGHT << "\n";
                // Continue anyway
            }

            GDALRasterBand* band = srcDS->GetRasterBand(1);
            double noData = band->GetNoDataValue();

            // Create 8‑bit single‑band output
            GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
            char** options = nullptr;
            options = CSLSetNameValue(options, "COMPRESS", "LZW");
            options = CSLSetNameValue(options, "PREDICTOR", "2");
            options = CSLSetNameValue(options, "TILED", "YES");
            options = CSLSetNameValue(options, "BLOCKXSIZE", "512");
            options = CSLSetNameValue(options, "BLOCKYSIZE", "512");

            GDALDataset* out8DS = drv->Create(out8.c_str(), width, height, 1, GDT_Byte, options);
            if (!out8DS) {
                std::cerr << "❌ Failed to create " << out8 << "\n";
                GDALClose(srcDS);
                return 1;
            }
            out8DS->SetProjection(srcDS->GetProjectionRef());
            double geoTransform[6];
            srcDS->GetGeoTransform(geoTransform);
            out8DS->SetGeoTransform(geoTransform);

            // Create colour (3‑band RGB) output
            GDALDataset* outColDS = drv->Create(outColor.c_str(), width, height, 3, GDT_Byte, options);
            if (!outColDS) {
                std::cerr << "❌ Failed to create " << outColor << "\n";
                GDALClose(srcDS);
                GDALClose(out8DS);
                return 1;
            }
            outColDS->SetProjection(srcDS->GetProjectionRef());
            outColDS->SetGeoTransform(geoTransform);

            // Build colour palette
            auto palette = buildPalette();

            // Process row by row
            std::vector<float> rowIn(width);
            std::vector<uint8_t> rowOut8(width);
            std::vector<uint8_t> rowR(width), rowG(width), rowB(width);

            std::cout << "🚀 Converting " << width << "x" << height << " pixels...\n";

            for (int y = 0; y < height; ++y) {
                // Read one scanline of float
                CPLErr err = band->RasterIO(GF_Read, 0, y, width, 1,
                                            rowIn.data(), width, 1, GDT_Float32,
                                            0, 0);
                if (err != CE_None) {
                    std::cerr << "❌ Error reading row " << y << "\n";
                    break;
                }

                // Compress and populate RGB
                for (int x = 0; x < width; ++x) {
                    float val = rowIn[x];
                    // Handle NoData: treat as water (0)
                    if (std::isnan(val) || (noData != -1 && std::abs(val - noData) < 1e-6f)) {
                        rowOut8[x] = 0;
                        // For colour, use water (0) -> blue
                        rowR[x] = 0;
                        rowG[x] = 0;
                        rowB[x] = 255;
                    } else {
                        uint8_t idx = compress(val);
                        rowOut8[x] = idx;
                        rowR[x] = palette[idx].r;
                        rowG[x] = palette[idx].g;
                        rowB[x] = palette[idx].b;
                    }
                }

                // Write 8‑bit band
                out8DS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1,
                                                   rowOut8.data(), width, 1, GDT_Byte,
                                                   0, 0);
                // Write RGB bands
                outColDS->GetRasterBand(1)->RasterIO(GF_Write, 0, y, width, 1,
                                                     rowR.data(), width, 1, GDT_Byte,
                                                     0, 0);
                outColDS->GetRasterBand(2)->RasterIO(GF_Write, 0, y, width, 1,
                                                     rowG.data(), width, 1, GDT_Byte,
                                                     0, 0);
                outColDS->GetRasterBand(3)->RasterIO(GF_Write, 0, y, width, 1,
                                                     rowB.data(), width, 1, GDT_Byte,
                                                     0, 0);

                if (y % 10000 == 0)
                    std::cout << "Row " << y << " / " << height << "\n";
            }

            GDALClose(outColDS);
            GDALClose(out8DS);
            GDALClose(srcDS);

            std::cout << "✅ Done!\n";
            std::cout << "  8‑bit map:  " << out8 << "\n";
            std::cout << "  Colour map: " << outColor << "\n";

            return 0;
        }
