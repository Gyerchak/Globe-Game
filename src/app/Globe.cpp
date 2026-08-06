#include "GlobeApp.h"

void GlobeApp::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void GlobeApp::initVulkan() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    loadSettings();

    createSwapChain();
    createImageViews();
    createRenderPass();

    createCommandPool();

    createOffscreenResources();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    prepareSphere();
    loadTexture();
    loadHeightmap();               // ← new
    loadGrid();
    createUniformBuffers();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

int main() {
    try {
        GlobeApp app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}