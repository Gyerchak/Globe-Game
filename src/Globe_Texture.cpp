#include "GlobeApp.h"

void GlobeApp::loadTexture() {
    std::string fname = "input/squarecolormap.tif";
    GDALAllRegister();
    GDALDataset* ds = (GDALDataset*)GDALOpen(fname.c_str(), GA_ReadOnly);
    if (!ds) throw std::runtime_error("Failed to open " + fname);

    size_t w = ds->GetRasterXSize();
    size_t h = ds->GetRasterYSize();
    std::cout << "📂 Texture: " << w << " x " << h << "\n";

    const int nBands = 3;
    std::vector<uint8_t> rgb(w * h * nBands);
    int bandMap[3] = {1, 2, 3};
    if (ds->RasterIO(GF_Read, 0, 0, (int)w, (int)h,
                     rgb.data(), (int)w, (int)h, GDT_Byte,
                     nBands, bandMap,
                     nBands, (int)(w * nBands), 1, nullptr) != CE_None) {
        GDALClose(ds);
        throw std::runtime_error("Failed to read texture pixels (RasterIO error)");
    }
    GDALClose(ds);

    std::vector<uint8_t> rgba(w * h * 4);
    for (size_t i = 0; i < w * h; ++i) {
        rgba[i*4 + 0] = rgb[i*3 + 0];
        rgba[i*4 + 1] = rgb[i*3 + 1];
        rgba[i*4 + 2] = rgb[i*3 + 2];
        rgba[i*4 + 3] = 255;
    }

    createImage((uint32_t)w, (uint32_t)h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
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
    while (y < (uint32_t)h) {
        uint32_t rows = std::min(maxRowsPerChunk, (uint32_t)h - y);
        VkDeviceSize chunkBytes = (VkDeviceSize)rows * w * 4;
        memcpy(mapped, rgba.data() + (size_t)y * w * 4, (size_t)chunkBytes);
        copyBufferToImageRegion(staging, textureImage, 0, y, (uint32_t)w, rows);
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

    size_t w = ds->GetRasterXSize();
    size_t h = ds->GetRasterYSize();
    std::cout << "📂 Heightmap: " << w << " x " << h << "\n";

    // Expect single band, read as 8-bit
    std::vector<uint8_t> pixels(w * h);
    if (ds->RasterIO(GF_Read, 0, 0, (int)w, (int)h,
                     pixels.data(), (int)w, (int)h, GDT_Byte,
                     1, nullptr,
                     0, 0, 0, nullptr) != CE_None) {
        GDALClose(ds);
        throw std::runtime_error("Failed to read heightmap pixels");
    }
    GDALClose(ds);

    createImage((uint32_t)w, (uint32_t)h, VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, heightmapImage, heightmapMemory);

    transitionImageLayout(heightmapImage, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Upload via staging buffer (same pattern as before)
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
    while (y < (uint32_t)h) {
        uint32_t rows = std::min(maxRowsPerChunk, (uint32_t)h - y);
        VkDeviceSize chunkBytes = (VkDeviceSize)rows * w;
        memcpy(mapped, pixels.data() + (size_t)y * w, (size_t)chunkBytes);
        copyBufferToImageRegion(staging, heightmapImage, 0, y, (uint32_t)w, rows);
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