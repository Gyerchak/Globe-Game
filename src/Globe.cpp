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

    loadSettings();                      // reads file/settings.cfg

    createSwapChain();
    createImageViews();
    createDepthResources();              // full size depth (not used)
    createRenderPass();                  // uses swapChainImageFormat

    createCommandPool();                 // <-- MUST be before any layout transitions

    createOffscreenResources();          // creates offscreen images + framebuffer
    createDescriptorSetLayout();
    createGraphicsPipeline();            // uses offscreenExtent for viewport
    prepareSphere();
    loadTexture();
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