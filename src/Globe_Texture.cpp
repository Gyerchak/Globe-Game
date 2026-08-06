#include "GlobeApp.h"

void GlobeApp::loadTexture() {
    std::string fname = "input/squarecolormap.tif";
    GDALAllRegister();
    GDALDataset* ds = (GDALDataset*)GDALOpen(fname.c_str(), GA_ReadOnly);
    if (!ds) throw std::runtime_error("Failed to open " + fname);

    uint32_t srcW = ds->GetRasterXSize();
    uint32_t srcH = ds->GetRasterYSize();
    std::cout << "📂 Texture: " << srcW << " x " << srcH << "\n";

    // Downscale if too large
    constexpr uint32_t MAX_DIM = 8192;
    uint32_t w = srcW, h = srcH;
    if (srcW > MAX_DIM || srcH > MAX_DIM) {
        float scale = (float)MAX_DIM / std::max(srcW, srcH);
        w = std::max(1u, (uint32_t)(srcW * scale));
        h = std::max(1u, (uint32_t)(srcH * scale));
        std::cout << "   Downscaling to: " << w << " x " << h << "\n";
    }

    std::vector<uint8_t> rgba(w * h * 4);
    int bandMap[3] = {1, 2, 3};
    CPLErr err = ds->RasterIO(GF_Read, 0, 0, srcW, srcH,
                               rgba.data(), w, h, GDT_Byte, 3, bandMap,
                               4, w * 4, 1, nullptr);
    if (err != CE_None) {
        GDALClose(ds);
        throw std::runtime_error("Failed to read texture pixels");
    }
    GDALClose(ds);

    for (size_t i = 0; i < w * h; ++i) rgba[i * 4 + 3] = 255;

    createImage(w, h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

    transitionImageLayout(textureImage, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    constexpr VkDeviceSize STAGING_SIZE = 64 * 1024 * 1024;
    VkBuffer staging; VkDeviceMemory stagingMem;
    createBuffer(STAGING_SIZE,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging, stagingMem);

    void* mapped;
    vkMapMemory(device, stagingMem, 0, STAGING_SIZE, 0, &mapped);

    uint32_t maxRowsPerChunk = (uint32_t)(STAGING_SIZE / (w * 4));
    if (maxRowsPerChunk == 0) maxRowsPerChunk = 1;

    uint32_t y = 0;
    while (y < h) {
        uint32_t rows = std::min(maxRowsPerChunk, h - y);
        VkDeviceSize chunkBytes = (VkDeviceSize)rows * w * 4;
        memcpy(mapped, rgba.data() + (size_t)y * w * 4, (size_t)chunkBytes);
        copyBufferToImageRegion(staging, textureImage, 0, y, w, rows);
        y += rows;
    }

    vkUnmapMemory(device, stagingMem);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    transitionImageLayout(textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM);
    textureSampler = createSampler();
}

void GlobeApp::loadHeightmap() {
    std::string fname = "input/heightmap.png";
    GDALAllRegister();
    GDALDataset* ds = (GDALDataset*)GDALOpen(fname.c_str(), GA_ReadOnly);
    if (!ds) throw std::runtime_error("Failed to open heightmap: " + fname);

    uint32_t w = ds->GetRasterXSize();
    uint32_t h = ds->GetRasterYSize();
    std::cout << "📂 Heightmap: " << w << " x " << h << "\n";

    std::vector<uint8_t> pixels(w * h);
    if (ds->RasterIO(GF_Read, 0, 0, w, h,
                     pixels.data(), w, h, GDT_Byte,
                     1, nullptr, 0, 0, 0, nullptr) != CE_None) {
        GDALClose(ds);
        throw std::runtime_error("Failed to read heightmap pixels");
    }
    GDALClose(ds);

    createImage(w, h, VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, heightmapImage, heightmapMemory);

    transitionImageLayout(heightmapImage, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    constexpr VkDeviceSize STAGING_SIZE = 64 * 1024 * 1024;
    VkBuffer staging; VkDeviceMemory stagingMem;
    createBuffer(STAGING_SIZE,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging, stagingMem);

    void* mapped;
    vkMapMemory(device, stagingMem, 0, STAGING_SIZE, 0, &mapped);

    uint32_t maxRowsPerChunk = (uint32_t)(STAGING_SIZE / w);
    if (maxRowsPerChunk == 0) maxRowsPerChunk = 1;

    uint32_t y = 0;
    while (y < h) {
        uint32_t rows = std::min(maxRowsPerChunk, h - y);
        VkDeviceSize chunkBytes = (VkDeviceSize)rows * w;
        memcpy(mapped, pixels.data() + (size_t)y * w, (size_t)chunkBytes);
        copyBufferToImageRegion(staging, heightmapImage, 0, y, w, rows);
        y += rows;
    }

    vkUnmapMemory(device, stagingMem);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    transitionImageLayout(heightmapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    heightmapView = createImageView(heightmapImage, VK_FORMAT_R8_UNORM);
    heightmapSampler = createSampler();
}