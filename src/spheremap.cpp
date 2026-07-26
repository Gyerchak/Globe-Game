// spheremap.cpp – memory‑efficient, parallelised
// Reads input/heightmapcolor.tif, produces circleprojection.tif
// Compile: g++ -std=c++20 -O3 -fopenmp spheremap.cpp -o exe/spheremap -lgdal
// Run: ./exe/spheremap

#include <gdal_priv.h>
#include <gdal_utils.h>
#include <cpl_conv.h>

#include <iostream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <algorithm>
#include <omp.h>

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// Bilinear interpolation from a flat interleaved RGB buffer
// -----------------------------------------------------------------------------
static inline void sampleBilinear(const uint8_t* src, int w, int h,
                                  float x, float y,
                                  uint8_t& r, uint8_t& g, uint8_t& b) {
    // Clamp to valid pixel range (with half‑pixel offset)
    if (x < 0.0f) x = 0.0f;
    if (x > w - 1.0f) x = w - 1.0f;
    if (y < 0.0f) y = 0.0f;
    if (y > h - 1.0f) y = h - 1.0f;

    int x0 = static_cast<int>(std::floor(x));
    int x1 = std::min(x0 + 1, w - 1);
    int y0 = static_cast<int>(std::floor(y));
    int y1 = std::min(y0 + 1, h - 1);

    float fx = x - x0;
    float fy = y - y0;
    float fx1 = 1.0f - fx;
    float fy1 = 1.0f - fy;

    // Pointers to the four corners
    const uint8_t* p00 = &src[(y0 * w + x0) * 3];
    const uint8_t* p10 = &src[(y0 * w + x1) * 3];
    const uint8_t* p01 = &src[(y1 * w + x0) * 3];
    const uint8_t* p11 = &src[(y1 * w + x1) * 3];

    auto interp = [&](int band) -> uint8_t {
        float v00 = p00[band];
        float v10 = p10[band];
        float v01 = p01[band];
        float v11 = p11[band];
        float val = fy1 * (fx1 * v00 + fx * v10) +
                    fy  * (fx1 * v01 + fx * v11);
        return static_cast<uint8_t>(std::round(val));
    };

    r = interp(0);
    g = interp(1);
    b = interp(2);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main() {
    GDALAllRegister();
    std::ios::sync_with_stdio(false);

    // Paths
    fs::path inPath = fs::current_path() / "input" / "heightmap_color.tif";
    fs::path outPath = fs::current_path() / "output" / "circleprojection.tif";

    if (!fs::exists(inPath)) {
        std::cerr << "❌ Input file not found: " << inPath << "\n";
        return 1;
    }
    fs::create_directories(outPath.parent_path());

    GDALDataset* src = (GDALDataset*)GDALOpen(inPath.c_str(), GA_ReadOnly);
    if (!src) {
        std::cerr << "❌ Could not open " << inPath << "\n";
        return 1;
    }

    int srcW = src->GetRasterXSize();
    int srcH = src->GetRasterYSize();
    std::cout << "📂 Source: " << srcW << " x " << srcH << "\n";

    // Read entire source into memory (interleaved RGB)
    std::cout << "📥 Reading source image into memory...\n";
    std::vector<uint8_t> srcBuf(srcW * srcH * 3);
    CPLErr err = src->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, srcW, srcH,
                                                 srcBuf.data(), srcW, srcH,
                                                 GDT_Byte, 3, 0);
    if (err != CE_None) {
        std::cerr << "❌ Failed to read source image.\n";
        GDALClose(src);
        return 1;
    }
    std::cout << "✅ Source loaded into memory.\n";

    // Determine output size
    const int outW = srcW;   // 65536
    const int outH = srcH;   // 32768
    const int S = outH;      // side of each square
    const float R = S / 2.0f;
    std::cout << "📐 Output: " << outW << " x " << outH << ", radius = " << R << "\n";

    // Create output dataset
    GDALDriver* drv = GetGDALDriverManager()->GetDriverByName("GTiff");
    char** opts = nullptr;
    opts = CSLSetNameValue(opts, "COMPRESS", "LZW");
    opts = CSLSetNameValue(opts, "PREDICTOR", "2");
    opts = CSLSetNameValue(opts, "TILED", "YES");
    opts = CSLSetNameValue(opts, "BLOCKXSIZE", "512");
    opts = CSLSetNameValue(opts, "BLOCKYSIZE", "512");

    GDALDataset* dst = drv->Create(outPath.c_str(), outW, outH, 3, GDT_Byte, opts);
    if (!dst) {
        std::cerr << "❌ Could not create " << outPath << "\n";
        GDALClose(src);
        return 1;
    }

    // Copy geotransform/projection
    double geoTransform[6];
    if (src->GetGeoTransform(geoTransform) == CE_None) {
        dst->SetGeoTransform(geoTransform);
        dst->SetProjection(src->GetProjectionRef());
    }
    GDALClose(src);

    // Buffers for each row (we'll process row by row, but in parallel)
    std::vector<uint8_t> rowR(outW), rowG(outW), rowB(outW);

    const double PI = 3.14159265358979323846;
    const float RAD_TO_DEG = 180.0f / static_cast<float>(PI);

    std::cout << "🔄 Transforming to circle projection (parallel)...\n";

    #pragma omp parallel for schedule(dynamic) private(rowR, rowG, rowB)
    for (int y = 0; y < outH; ++y) {
        // Each thread gets its own row buffers
        std::vector<uint8_t> lR(outW), lG(outW), lB(outW);

        for (int x = 0; x < outW; ++x) {
            int sqIdx = (x >= S) ? 1 : 0;
            float cx = (sqIdx == 0) ? S/2.0f : S + S/2.0f;
            float cy = S/2.0f;

            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx*dx + dy*dy);

            if (dist > R) {
                lR[x] = 0; lG[x] = 0; lB[x] = 0;
                continue;
            }

            float angle = std::atan2(dy, dx);
            float lon = angle * RAD_TO_DEG;
            if (lon < -180.0f) lon += 360.0f;
            if (lon > 180.0f)  lon -= 360.0f;

            float lat;
            if (sqIdx == 0) {
                lat = 90.0f - (dist / R) * 90.0f;
            } else {
                lat = -90.0f + (dist / R) * 90.0f;
            }

            if (lat < -90.0f) lat = -90.0f;
            if (lat > 90.0f)  lat = 90.0f;

            float srcX = (lon + 180.0f) / 360.0f * (srcW - 1);
            float srcY = (90.0f - lat) / 180.0f * (srcH - 1);

            uint8_t r, g, b;
            sampleBilinear(srcBuf.data(), srcW, srcH, srcX, srcY, r, g, b);
            lR[x] = r; lG[x] = g; lB[x] = b;
        }

        // Write row to output (critical section)
        #pragma omp critical
        {
            dst->GetRasterBand(1)->RasterIO(GF_Write, 0, y, outW, 1,
                                            lR.data(), outW, 1, GDT_Byte, 0, 0);
            dst->GetRasterBand(2)->RasterIO(GF_Write, 0, y, outW, 1,
                                            lG.data(), outW, 1, GDT_Byte, 0, 0);
            dst->GetRasterBand(3)->RasterIO(GF_Write, 0, y, outW, 1,
                                            lB.data(), outW, 1, GDT_Byte, 0, 0);
        }

        if (y % 10000 == 0) {
            #pragma omp critical
            std::cout << "  row " << y << " / " << outH << "\n";
        }
    }

    GDALClose(dst);
    std::cout << "✅ Done! Output written to " << outPath << "\n";
    return 0;
}