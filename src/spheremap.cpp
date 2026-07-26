// spheremap.cpp – memory‑aware, with fallback to tiled processing
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
#include <memory>
#include <omp.h>

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// Bilinear interpolation from flat interleaved RGB buffer
// -----------------------------------------------------------------------------
static inline void sampleBilinear(const uint8_t* src, int w, int h,
                                  float x, float y,
                                  uint8_t& r, uint8_t& g, uint8_t& b) {
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

    // ---------- Try to allocate full image buffer ----------
    size_t totalPixels = static_cast<size_t>(srcW) * srcH;
    std::unique_ptr<uint8_t[]> srcBuf;
    bool useMemory = false;

    try {
        srcBuf.reset(new uint8_t[totalPixels * 3]);
        std::cout << "📥 Reading source image into memory...\n";
        CPLErr err = src->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, srcW, srcH,
                                                     srcBuf.get(), srcW, srcH,
                                                     GDT_Byte, 3, 0);
        if (err == CE_None) {
            useMemory = true;
            std::cout << "✅ Source loaded into memory.\n";
        } else {
            std::cerr << "❌ Failed to read source image.\n";
            GDALClose(src);
            return 1;
        }
    } catch (const std::bad_alloc& e) {
        std::cerr << "⚠️  Memory allocation failed (" << e.what()
                  << "). Falling back to block processing.\n";
        useMemory = false;
    }

    // ---------- Output parameters ----------
    const int outW = srcW;
    const int outH = srcH;
    const int S = outH;
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

    double geoTransform[6];
    if (src->GetGeoTransform(geoTransform) == CE_None) {
        dst->SetGeoTransform(geoTransform);
        dst->SetProjection(src->GetProjectionRef());
    }

    const double PI = 3.14159265358979323846;
    const float RAD_TO_DEG = 180.0f / static_cast<float>(PI);

    if (useMemory) {
        // ---------------------------------------------------------------
        // Fast path: process from memory, parallelised
        // ---------------------------------------------------------------
        std::cout << "🔄 Transforming (memory mode, parallel)...\n";
        #pragma omp parallel for schedule(dynamic)
        for (int y = 0; y < outH; ++y) {
            std::vector<uint8_t> lR(outW), lG(outW), lB(outW);

            for (int x = 0; x < outW; ++x) {
                int sqIdx = (x >= S) ? 1 : 0;
                float cx = (sqIdx == 0) ? S/2.0f : S + S/2.0f;
                float cy = S/2.0f;

                float dx = x - cx;
                float dy = y - cy;
                float dist = std::sqrt(dx*dx + dy*dy);

                if (dist > R) {
                    lR[x] = lG[x] = lB[x] = 0;
                    continue;
                }

                float angle = std::atan2(dy, dx);
                float lon = angle * RAD_TO_DEG;
                if (lon < -180.0f) lon += 360.0f;
                if (lon > 180.0f)  lon -= 360.0f;

                float lat = (sqIdx == 0) ? 90.0f - (dist / R) * 90.0f
                                        : -90.0f + (dist / R) * 90.0f;
                if (lat < -90.0f) lat = -90.0f;
                if (lat > 90.0f)  lat = 90.0f;

                float srcX = (lon + 180.0f) / 360.0f * (srcW - 1);
                float srcY = (90.0f - lat) / 180.0f * (srcH - 1);

                uint8_t r, g, b;
                sampleBilinear(srcBuf.get(), srcW, srcH, srcX, srcY, r, g, b);
                lR[x] = r; lG[x] = g; lB[x] = b;
            }

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
    } else {
        // ---------------------------------------------------------------
        // Fallback: process row by row using GDAL RasterIO (slower but works)
        // ---------------------------------------------------------------
        std::cout << "🔄 Transforming (block mode, single-threaded)...\n";
        std::vector<uint8_t> rowSrc(srcW * 3);
        std::vector<uint8_t> rowR(outW), rowG(outW), rowB(outW);

        for (int y = 0; y < outH; ++y) {
            // We need source pixels for many possible coordinates.
            // Since we can't hold everything in memory, we process each output
            // pixel by reading source individually – slow but memory‑safe.
            // To improve speed, we read the whole source row‑wise, but we need
            // random access. So we'll read chunks.
            // For simplicity, we'll do per‑pixel RasterIO (which is slow but works).
            // A better approach would be to read blocks of source, but that
            // complicates the code. Given this is a fallback, we accept it.

            for (int x = 0; x < outW; ++x) {
                int sqIdx = (x >= S) ? 1 : 0;
                float cx = (sqIdx == 0) ? S/2.0f : S + S/2.0f;
                float cy = S/2.0f;

                float dx = x - cx;
                float dy = y - cy;
                float dist = std::sqrt(dx*dx + dy*dy);

                if (dist > R) {
                    rowR[x] = rowG[x] = rowB[x] = 0;
                    continue;
                }

                float angle = std::atan2(dy, dx);
                float lon = angle * RAD_TO_DEG;
                if (lon < -180.0f) lon += 360.0f;
                if (lon > 180.0f)  lon -= 360.0f;

                float lat = (sqIdx == 0) ? 90.0f - (dist / R) * 90.0f
                                        : -90.0f + (dist / R) * 90.0f;
                if (lat < -90.0f) lat = -90.0f;
                if (lat > 90.0f)  lat = 90.0f;

                float srcX = (lon + 180.0f) / 360.0f * (srcW - 1);
                float srcY = (90.0f - lat) / 180.0f * (srcH - 1);

                // Read 2x2 neighbourhood from source
                int x0 = static_cast<int>(std::floor(srcX));
                int x1 = std::min(x0 + 1, srcW - 1);
                int y0 = static_cast<int>(std::floor(srcY));
                int y1 = std::min(y0 + 1, srcH - 1);

                uint8_t buf[12]; // 2x2 pixels, 3 channels each
                // Read 2x2 block from source
                CPLErr err = src->GetRasterBand(1)->RasterIO(GF_Read, x0, y0, 2, 2,
                                                             buf, 2, 2, GDT_Byte,
                                                             3, 0);
                if (err != CE_None) {
                    rowR[x] = rowG[x] = rowB[x] = 0;
                    continue;
                }

                // Bilinear interpolation
                float fx = srcX - x0;
                float fy = srcY - y0;
                float fx1 = 1.0f - fx;
                float fy1 = 1.0f - fy;

                auto interp = [&](int band) -> uint8_t {
                    float v00 = buf[(0*2 + 0)*3 + band];
                    float v10 = buf[(0*2 + 1)*3 + band];
                    float v01 = buf[(1*2 + 0)*3 + band];
                    float v11 = buf[(1*2 + 1)*3 + band];
                    float val = fy1 * (fx1 * v00 + fx * v10) +
                                fy  * (fx1 * v01 + fx * v11);
                    return static_cast<uint8_t>(std::round(val));
                };

                rowR[x] = interp(0);
                rowG[x] = interp(1);
                rowB[x] = interp(2);
            }

            // Write row
            dst->GetRasterBand(1)->RasterIO(GF_Write, 0, y, outW, 1,
                                            rowR.data(), outW, 1, GDT_Byte, 0, 0);
            dst->GetRasterBand(2)->RasterIO(GF_Write, 0, y, outW, 1,
                                            rowG.data(), outW, 1, GDT_Byte, 0, 0);
            dst->GetRasterBand(3)->RasterIO(GF_Write, 0, y, outW, 1,
                                            rowB.data(), outW, 1, GDT_Byte, 0, 0);

            if (y % 10000 == 0) {
                std::cout << "  row " << y << " / " << outH << "\n";
            }
        }
    }

    GDALClose(dst);
    GDALClose(src);
    std::cout << "✅ Done! Output written to " << outPath << "\n";
    return 0;
}