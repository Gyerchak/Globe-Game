#include "GlobeApp.h"

void GlobeApp::createOffscreenResources() {
    // Create 16-bit colour image
    createImage(swapChainExtent.width, swapChainExtent.height,
                offscreenFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                offscreenImage, offscreenImageMemory);

    offscreenImageView = createImageView(offscreenImage, offscreenFormat,
                                         VK_IMAGE_ASPECT_COLOR_BIT);

    // Transition to colour attachment layout (initial)
    transitionImageLayout(offscreenImage,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_ASPECT_COLOR_BIT);

    // Create framebuffer with off‑screen colour + depth
    VkImageView attachments[2] = {offscreenImageView, depthImageView};
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = renderPass;   // must match the render pass format
    fbInfo.attachmentCount = 2;
    fbInfo.pAttachments = attachments;
    fbInfo.width = swapChainExtent.width;
    fbInfo.height = swapChainExtent.height;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &offscreenFramebuffer) != VK_SUCCESS)
        throw std::runtime_error("failed to create off‑screen framebuffer!");
}