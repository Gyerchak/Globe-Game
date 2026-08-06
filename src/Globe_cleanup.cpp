#include "GlobeApp.h"

void GlobeApp::cleanup() {
    saveGridToFile();

    if (device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device);

    // Sync objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (renderFinishedSemaphores[i]) vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        if (inFlightFences[i]) vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);

    // Uniform buffers
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (uniformBuffers[i]) vkDestroyBuffer(device, uniformBuffers[i], nullptr);
        if (uniformBuffersMemory[i]) vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
    }

    // Texture
    if (textureSampler) vkDestroySampler(device, textureSampler, nullptr);
    if (textureImageView) vkDestroyImageView(device, textureImageView, nullptr);
    if (textureImage) vkDestroyImage(device, textureImage, nullptr);
    if (textureImageMemory) vkFreeMemory(device, textureImageMemory, nullptr);

    // Grid
    if (gridSampler) vkDestroySampler(device, gridSampler, nullptr);
    if (gridImageView) vkDestroyImageView(device, gridImageView, nullptr);
    if (gridImage) vkDestroyImage(device, gridImage, nullptr);
    if (gridImageMemory) vkFreeMemory(device, gridImageMemory, nullptr);

    // Offscreen resources
    destroyOffscreenResources();

    if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    if (graphicsPipeline) vkDestroyPipeline(device, graphicsPipeline, nullptr);
    if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    if (descriptorSetLayout) vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);

    for (auto& iv : swapChainImageViews) if (iv) vkDestroyImageView(device, iv, nullptr);
    if (swapChain) vkDestroySwapchainKHR(device, swapChain, nullptr);
    if (vertexBuffer) vkDestroyBuffer(device, vertexBuffer, nullptr);
    if (vertexBufferMemory) vkFreeMemory(device, vertexBufferMemory, nullptr);
    if (indexBuffer) vkDestroyBuffer(device, indexBuffer, nullptr);
    if (indexBufferMemory) vkFreeMemory(device, indexBufferMemory, nullptr);

    vkDestroyDevice(device, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
    if (window) { glfwDestroyWindow(window); glfwTerminate(); }
}