#include "GlobeApp.h"

void GlobeApp::createSwapChain() {
    SwapChainSupport ss = querySwapChainSupport(physicalDevice);
    VkSurfaceFormatKHR fmt = chooseSwapSurfaceFormat(ss.formats);
    VkPresentModeKHR mode = chooseSwapPresentMode(ss.modes);
    VkExtent2D ext = chooseSwapExtent(ss.caps);
    uint32_t imgCount = ss.caps.minImageCount + 1;
    if (ss.caps.maxImageCount > 0 && imgCount > ss.caps.maxImageCount) imgCount = ss.caps.maxImageCount;
    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = surface;
    sci.minImageCount = imgCount;
    sci.imageFormat = fmt.format;
    sci.imageColorSpace = fmt.colorSpace;
    sci.imageExtent = ext;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    auto idx = findQueueFamilies(physicalDevice);
    uint32_t fams[] = {idx.graphics.value(), idx.present.value()};
    if (idx.graphics != idx.present) {
        sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices = fams;
    } else sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = ss.caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = mode;
    sci.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(device, &sci, nullptr, &swapChain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain!");
    vkGetSwapchainImagesKHR(device, swapChain, &imgCount, nullptr);
    swapChainImages.resize(imgCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imgCount, swapChainImages.data());
    swapChainImageFormat = fmt.format;
    swapChainExtent = ext;
}

VkSurfaceFormatKHR GlobeApp::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& avail) {
    for (auto& f : avail) {
        if (f.format == VK_FORMAT_B5G5R5A1_UNORM_PACK16 &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return avail[0];
}

VkPresentModeKHR GlobeApp::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& avail) {
    for (auto& m : avail)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D GlobeApp::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps) {
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    return { (uint32_t)w, (uint32_t)h };
}

void GlobeApp::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); ++i)
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat);
}

void GlobeApp::createDepthResources() {
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depthImage, depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}