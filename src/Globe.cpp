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

    // Check if B5G6R5 can be used as a colour attachment
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, offscreenFormat, &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
        throw std::runtime_error(
            "VK_FORMAT_B5G6R5_UNORM_PACK16 not supported as a colour attachment! "
            "Your GPU cannot render to this format. "
            "Try using a lower resolution off‑screen target (Option B) or a different format.");
    }
    std::cout << "✅ B5G6R5 colour attachment supported\n";

    createSwapChain();
    createImageViews();
    createDepthResources();
    createOffscreenResources();          // <-- new
    createRenderPass();                  // depends on offscreenFormat
    createDescriptorSetLayout();
    createGraphicsPipeline();
    // createFramebuffers() is a no-op
    createCommandPool();
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