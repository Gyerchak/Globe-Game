#include "GlobeApp.h"

void GlobeApp::loadGrid() {
    const size_t gridW = 32768, gridH = 32768;
    const size_t gridSize = gridW * gridH;

    gridData.resize(gridSize, 0);

    const std::string gridFile = "32kbitmap.bin";
    std::ifstream fin(gridFile, std::ios::binary);
    if (fin) {
        fin.read((char*)gridData.data(), gridSize);
        size_t read = fin.gcount();
        if (read < gridSize) {
            std::cout << "⚠️  Grid file too small, filling remainder with zeros.\n";
            std::fill(gridData.begin() + read, gridData.end(), 0);
        }
        std::cout << "📥 Loaded grid from " << gridFile << "\n";
    } else {
        std::ofstream fout(gridFile, std::ios::binary);
        if (fout) {
            std::vector<char> zeros(gridSize, 0);
            fout.write(zeros.data(), gridSize);
            std::cout << "📁 Created new grid file: " << gridFile << " (filled with zeros)\n";
        } else {
            std::cerr << "⚠️  Could not create grid file, running with RAM-only zeros.\n";
        }
    }

    createImage((uint32_t)gridW, (uint32_t)gridH, VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gridImage, gridImageMemory);

    transitionImageLayout(gridImage, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    constexpr VkDeviceSize STAGING_SIZE = 64 * 1024 * 1024;
    VkBuffer staging; VkDeviceMemory stagingMem;
    createBuffer(STAGING_SIZE,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging, stagingMem);

    void* mapped;
    vkMapMemory(device, stagingMem, 0, STAGING_SIZE, 0, &mapped);

    uint32_t maxRowsPerChunk = (uint32_t)(STAGING_SIZE / gridW);
    if (maxRowsPerChunk == 0) maxRowsPerChunk = 1;

    uint32_t y = 0;
    while (y < gridH) {
        uint32_t rows = std::min(maxRowsPerChunk, gridH - y);
        VkDeviceSize chunkBytes = (VkDeviceSize)rows * gridW;
        memcpy(mapped, gridData.data() + (size_t)y * gridW, (size_t)chunkBytes);
        copyBufferToImageRegion(staging, gridImage, 0, y, (uint32_t)gridW, rows);
        y += rows;
    }

    vkUnmapMemory(device, stagingMem);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    transitionImageLayout(gridImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    gridImageView = createImageView(gridImage, VK_FORMAT_R8_UNORM);
    gridSampler = createSampler();
}

void GlobeApp::saveGridToFile() {
    const std::string gridFile = "32kbitmap.bin";
    std::ofstream fout(gridFile, std::ios::binary | std::ios::trunc);
    if (fout) {
        fout.write((const char*)gridData.data(), gridData.size());
        std::cout << "💾 Grid saved to " << gridFile << "\n";
    } else {
        std::cerr << "⚠️  Failed to save grid to " << gridFile << "\n";
    }
}