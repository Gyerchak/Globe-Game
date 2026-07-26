// spheremap.cpp
// Reads input/heightmapcolor.tif (2:1 equirectangular, RGB)
// Produces circleprojection.tif (two disks: north & south hemispheres)
// Compile: g++ -std=c++20 -O3 spheremap.cpp -o exe/spheremap -lgdal
// Run: ./exe/spheremap

#include <gdal_priv.h>
#include <gdal_utils.h>
#include <cpl_conv.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <algorithm>

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// Bilinear interpolation for 3‑band uint8
// -----------------------------------------------------------------------------
static void sampleBilinear(GDALDataset* src, float x, float y,
                           uint8_t& r, uint8_t& g, uint8_t& b) {
    int w = src->GetRasterXSize();
    int h = src->GetRasterYSize();

    // Clamp to valid pixel range (with half-pixel offset)
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

    std::vector<uint8_t> buf(3 * 4); // 4 pixels, each 3 bands
    CPLErr err = src->GetRasterBand(1)->RasterIO(GF_Read, x0, y0, 2, 2,
                                                  buf.data(), 2, 2, GDT_Byte,
                                                  3, 0);
    if (err != CE_None) {
        r = g = b = 0;
        return;
    }

    // Interpolate each channel
    auto interp = [&](int band) -> uint8_t {
        float p00 = buf[(0*2 + 0)*3 + band];
        float p10 = buf[(0*2 + 1)*3 + band];
        float p01 = buf[(1*2 + 0)*3 + band];
        float p11 = buf[(1*2 + 1)*3 + band];
        float val = fy1 * (fx1 * p00 + fx * p10) +
                    fy  * (fx1 * p01 + fx * p11);
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
    fs::path inPath = fs::current_path() / "input" / "heightmapcolor.tif";
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
    if (srcW != 65536 || srcH != 32768) {
        std::cerr << "⚠️  Input size " << srcW << "x" << srcH
                  << " – expected 65536x32768. Continuing anyway.\n";
    }

    // Determine output size: same as input (2:1)
    const int outW = srcW;   // 65536
    const int outH = srcH;   // 32768
    const int S = outH;      // side of each square
    const float R = S / 2.0f; // radius of each circle

    std::cout << "📐 Output: " << outW << " x " << outH << "\n";
    std::cout << "   Each circle radius = " << R << " pixels\n";

    // Create output dataset (3‑band Byte, LZW compression)
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

    // Copy georeferencing (if any)
    double geoTransform[6];
    if (src->GetGeoTransform(geoTransform) == CE_None) {
        dst->SetGeoTransform(geoTransform);
        dst->SetProjection(src->GetProjectionRef());
    }

    // Buffers for each row
    std::vector<uint8_t> rowR(outW), rowG(outW), rowB(outW);

    // Constants
    const double PI = 3.14159265358979323846;
    const float RAD_TO_DEG = 180.0f / static_cast<float>(PI);

    std::cout << "🔄 Transforming to circle projection...\n";
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            // Determine which square we're in
            int sqIdx = (x >= S) ? 1 : 0;  // 0 = north (left), 1 = south (right)
            float cx = (sqIdx == 0) ? S/2.0f : S + S/2.0f;
            float cy = S/2.0f;

            float dx = x - cx;
            float dy = y - cy;
            float dist = std::sqrt(dx*dx + dy*dy);

            if (dist > R) {
                // Outside circle – black
                rowR[x] = 0;
                rowG[x] = 0;
                rowB[x] = 0;
                continue;
            }

            // Angle in radians, convert to degrees (0..360)
            float angle = std::atan2(dy, dx); // range -PI..PI
            float lon = angle * RAD_TO_DEG;   // -180..180
            if (lon < -180.0f) lon += 360.0f;
            if (lon > 180.0f)  lon -= 360.0f;

            // Latitude: linear from pole at centre to equator at edge
            float lat;
            if (sqIdx == 0) { // North
                // dist=0 -> lat=90, dist=R -> lat=0
                lat = 90.0f - (dist / R) * 90.0f;
            } else { // South
                // dist=0 -> lat=-90, dist=R -> lat=0
                lat = -90.0f + (dist / R) * 90.0f;
            }

            // Clamp
            if (lat < -90.0f) lat = -90.0f;
            if (lat > 90.0f)  lat = 90.0f;

            // Convert lat/lon to source pixel coordinates
            // Input equirectangular: lon -180..180 maps to x 0..srcW-1
            // lat 90..-90 maps to y 0..srcH-1 (top = north)
            float srcX = (lon + 180.0f) / 360.0f * (srcW - 1);
            float srcY = (90.0f - lat) / 180.0f * (srcH - 1);

            // Sample with bilinear interpolation
            uint8_t r, g, b;
            sampleBilinear(src, srcX, srcY, r, g, b);
            rowR[x] = r;
            rowG[x] = g;
            rowB[x] = b;
        }

        // Write row to output
        dst->GetRasterBand(1)->RasterIO(GF_Write, 0, y, outW, 1,
                                        rowR.data(), outW, 1, GDT_Byte, 0, 0);
        dst->GetRasterBand(2)->RasterIO(GF_Write, 0, y, outW, 1,
                                        rowG.data(), outW, 1, GDT_Byte, 0, 0);
        dst->GetRasterBand(3)->RasterIO(GF_Write, 0, y, outW, 1,
                                        rowB.data(), outW, 1, GDT_Byte, 0, 0);

        if (y % 10000 == 0)
            std::cout << "  row " << y << " / " << outH << "\n";
    }

    GDALClose(dst);
    GDALClose(src);

    std::cout << "✅ Done! Output written to " << outPath << "\n";
    return 0;
}