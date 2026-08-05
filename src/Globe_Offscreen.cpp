#include "GlobeApp.h"

void GlobeApp::createOffscreenResources() {
    // The offscreen color image must match the render pass format (swapChainImageFormat)
    VkFormat colorFormat = swapChainImageFormat;   // e.g. VK_FORMAT_B8G8R8A8_SRGB

    createImage(offscreenExtent.width, offscreenExtent.height,
                colorFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                offscreenColorImage, offscreenColorMemory);
    offscreenColorView = createImageView(offscreenColorImage, colorFormat);

    // Depth attachment (matching size)
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    createImage(offscreenExtent.width, offscreenExtent.height,
                depthFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                offscreenDepthImage, offscreenDepthMemory);
    offscreenDepthView = createImageView(offscreenDepthImage, depthFormat,
                                         VK_IMAGE_ASPECT_DEPTH_BIT);

    // Transition to appropriate layouts (initial)
    transitionImageLayout(offscreenColorImage,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_ASPECT_COLOR_BIT);
    transitionImageLayout(offscreenDepthImage,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_ASPECT_DEPTH_BIT);

    // Create framebuffer
    VkImageView attachments[2] = {offscreenColorView, offscreenDepthView};
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = renderPass;
    fbInfo.attachmentCount = 2;
    fbInfo.pAttachments = attachments;
    fbInfo.width = offscreenExtent.width;
    fbInfo.height = offscreenExtent.height;
    fbInfo.layers = 1;
    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &offscreenFramebuffer) != VK_SUCCESS)
        throw std::runtime_error("failed to create off‑screen framebuffer!");
}

void GlobeApp::destroyOffscreenResources() {
    if (offscreenFramebuffer) vkDestroyFramebuffer(device, offscreenFramebuffer, nullptr);
    if (offscreenColorView) vkDestroyImageView(device, offscreenColorView, nullptr);
    if (offscreenColorImage) vkDestroyImage(device, offscreenColorImage, nullptr);
    if (offscreenColorMemory) vkFreeMemory(device, offscreenColorMemory, nullptr);
    if (offscreenDepthView) vkDestroyImageView(device, offscreenDepthView, nullptr);
    if (offscreenDepthImage) vkDestroyImage(device, offscreenDepthImage, nullptr);
    if (offscreenDepthMemory) vkFreeMemory(device, offscreenDepthMemory, nullptr);
}