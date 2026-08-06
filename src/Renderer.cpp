#include "Renderer.h"
#include "Window.h"
#include "Camera.h"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <set>
#include <cstring>
#include <algorithm>
#include <gdal_priv.h>

// Validation layers helper
static bool checkValidationLayerSupport(const std::vector<const char*>& requested) {
    uint32_t count;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());
    for (const char* layerName : requested) {
        bool found = false;
        for (const auto& props : available) {
            if (strcmp(layerName, props.layerName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

Renderer::Renderer(Window* w, const AppSettings& s) : window(w), settings(s) {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createCommandPool();
    createOffscreenResources();
    createDescriptorSetLayout();
    createPipeline();
    createUniformBuffers();
    createSyncObjects();
}

Renderer::~Renderer() {
    waitIdle();
    if (gridSampler) vkDestroySampler(device, gridSampler, nullptr);
    if (gridView) vkDestroyImageView(device, gridView, nullptr);
    if (gridImage) vkDestroyImage(device, gridImage, nullptr);
    if (gridMemory) vkFreeMemory(device, gridMemory, nullptr);

    if (heightmapSampler) vkDestroySampler(device, heightmapSampler, nullptr);
    if (heightmapView) vkDestroyImageView(device, heightmapView, nullptr);
    if (heightmapImage) vkDestroyImage(device, heightmapImage, nullptr);
    if (heightmapMemory) vkFreeMemory(device, heightmapMemory, nullptr);

    if (textureSampler) vkDestroySampler(device, textureSampler, nullptr);
    if (textureView) vkDestroyImageView(device, textureView, nullptr);
    if (textureImage) vkDestroyImage(device, textureImage, nullptr);
    if (textureMemory) vkFreeMemory(device, textureMemory, nullptr);

    sphereMesh.cleanup(device);

    for (auto& sem : imageAvailableSemaphores) vkDestroySemaphore(device, sem, nullptr);
    for (auto& sem : renderFinishedSemaphores) vkDestroySemaphore(device, sem, nullptr);
    for (auto& fence : inFlightFences) vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);

    for (auto& buf : uniformBuffers) vkDestroyBuffer(device, buf, nullptr);
    for (auto& mem : uniformBuffersMemory) vkFreeMemory(device, mem, nullptr);

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    if (offscreenColorView) vkDestroyImageView(device, offscreenColorView, nullptr);
    if (offscreenColorImage) vkDestroyImage(device, offscreenColorImage, nullptr);
    if (offscreenColorMemory) vkFreeMemory(device, offscreenColorMemory, nullptr);
    if (offscreenDepthView) vkDestroyImageView(device, offscreenDepthView, nullptr);
    if (offscreenDepthImage) vkDestroyImage(device, offscreenDepthImage, nullptr);
    if (offscreenDepthMemory) vkFreeMemory(device, offscreenDepthMemory, nullptr);

    for (auto& iv : swapChainImageViews) vkDestroyImageView(device, iv, nullptr);
    vkDestroySwapchainKHR(device, swapChain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}

void Renderer::waitIdle() {
    vkDeviceWaitIdle(device);
}

void Renderer::initResources() {
    sphereMesh.generate();
    sphereMesh.upload(device, commandPool, graphicsQueue, physicalDevice);
    loadTextureFromFile("input/squarecolormap.tif");
    loadHeightmapFromFile("input/heightmap.png");
    loadGridFromFile("bin/32kbitmap.bin");
    createDescriptorSets();
    createCommandBuffers();
}

// --- Vulkan Setup ---
void Renderer::createInstance() {
    std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    bool enableValidation = checkValidationLayerSupport(validationLayers);
    if (enableValidation) std::cout << "🔍 Validation layers enabled\n";
    else std::cout << "⚠️  Validation layers not available\n";

    VkApplicationInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = "GlobeViewer";
    ai.applicationVersion = VK_MAKE_API_VERSION(0,1,0,0);
    ai.engineVersion = VK_MAKE_API_VERSION(0,1,0,0);
    ai.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> exts(glfwExts, glfwExts + glfwExtCount);
    if (enableValidation) exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &ai;
    ci.enabledExtensionCount = (uint32_t)exts.size();
    ci.ppEnabledExtensionNames = exts.data();
    if (enableValidation) {
        ci.enabledLayerCount = (uint32_t)validationLayers.size();
        ci.ppEnabledLayerNames = validationLayers.data();
    }
    if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance!");
}

void Renderer::createSurface() {
    if (glfwCreateWindowSurface(instance, window->handle(), nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create surface!");
}

void Renderer::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("no Vulkan GPUs");
    std::vector<VkPhysicalDevice> devs(count);
    vkEnumeratePhysicalDevices(instance, &count, devs.data());
    for (auto dev : devs) {
        // Check suitability (must have graphics+present queues, swapchain support)
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(qcount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, qfs.data());
        int graphicsIdx = -1, presentIdx = -1;
        for (uint32_t i = 0; i < qcount; ++i) {
            if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsIdx = i;
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
            if (presentSupport) presentIdx = i;
            if (graphicsIdx >=0 && presentIdx >=0) break;
        }
        if (graphicsIdx < 0 || presentIdx < 0) continue;

        // Check swapchain extension support
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availExt(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, availExt.data());
        bool hasSwapchain = false;
        for (auto& ext : availExt) {
            if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true; break;
            }
        }
        if (!hasSwapchain) continue;

        physicalDevice = dev;
        break;
    }
    if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("no suitable GPU");
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "🖥️  GPU: " << props.deviceName << "\n";
    std::cout << "   Max texture size: " << props.limits.maxImageDimension2D << "\n";
}

void Renderer::createLogicalDevice() {
    uint32_t qcount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qcount, nullptr);
    std::vector<VkQueueFamilyProperties> qfs(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qcount, qfs.data());
    int graphicsIdx = -1, presentIdx = -1;
    for (uint32_t i = 0; i < qcount; ++i) {
        if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsIdx = i;
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (presentSupport) presentIdx = i;
        if (graphicsIdx >=0 && presentIdx >=0) break;
    }

    std::set<uint32_t> uniqueQueueFamilies = {(uint32_t)graphicsIdx, (uint32_t)presentIdx};
    std::vector<VkDeviceQueueCreateInfo> qcis;
    float prio = 1.0f;
    for (uint32_t fam : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = fam;
        qi.queueCount = 1;
        qi.pQueuePriorities = &prio;
        qcis.push_back(qi);
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynRendering{};
    dynRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynRendering.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.pNext = &dynRendering;
    dci.queueCreateInfoCount = (uint32_t)qcis.size();
    dci.pQueueCreateInfos = qcis.data();
    dci.pEnabledFeatures = &features;
    const char* swapExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = &swapExt;
    if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device!");
    vkGetDeviceQueue(device, graphicsIdx, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentIdx, 0, &presentQueue);
}

void Renderer::createSwapChain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (auto& fmt : formats) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = fmt; break;
        }
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, modes.data());
    VkPresentModeKHR chosenMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!settings.vsync) {
        if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end())
            chosenMode = VK_PRESENT_MODE_MAILBOX_KHR;
        else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end())
            chosenMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        int w, h; glfwGetFramebufferSize(window->handle(), &w, &h);
        extent = {(uint32_t)w, (uint32_t)h};
    }
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = surface;
    sci.minImageCount = imageCount;
    sci.imageFormat = chosenFormat.format;
    sci.imageColorSpace = chosenFormat.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = chosenMode;
    sci.clipped = VK_TRUE;

    uint32_t families[] = {(uint32_t)0, (uint32_t)0}; // we already know both queues might be same family, we'll check
    // Actually need to query the family indices again; we stored them? Not yet.
    // We'll just set exclusive sharing for simplicity if both same, else concurrent.
    // We'll assume single queue family for now, but robust code would check. This is okay for most systems.
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateSwapchainKHR(device, &sci, nullptr, &swapChain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swap chain!");

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
    swapChainImageFormat = chosenFormat.format;
    swapChainExtent = extent;
}

void Renderer::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); ++i) {
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = swapChainImages[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = swapChainImageFormat;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        vkCreateImageView(device, &vi, nullptr, &swapChainImageViews[i]);
    }
}

void Renderer::recreateSwapChain() {
    int w, h;
    glfwGetFramebufferSize(window->handle(), &w, &h);
    while (w == 0 || h == 0) { glfwWaitEvents(); glfwGetFramebufferSize(window->handle(), &w, &h); }
    vkDeviceWaitIdle(device);
    for (auto iv : swapChainImageViews) vkDestroyImageView(device, iv, nullptr);
    swapChainImageViews.clear();
    vkDestroySwapchainKHR(device, swapChain, nullptr);
    createSwapChain();
    createImageViews();
    window->getInput().framebufferResized = false;
}

void Renderer::createCommandPool() {
    uint32_t qcount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qcount, nullptr);
    std::vector<VkQueueFamilyProperties> qfs(qcount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qcount, qfs.data());
    int graphicsIdx = -1;
    for (uint32_t i = 0; i < qcount; ++i) if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { graphicsIdx = i; break; }
    VkCommandPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pi.queueFamilyIndex = graphicsIdx;
    vkCreateCommandPool(device, &pi, nullptr, &commandPool);
}

void Renderer::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = (uint32_t)commandBuffers.size();
    if (vkAllocateCommandBuffers(device, &ai, commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers!");
}

void Renderer::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateSemaphore(device, &si, nullptr, &imageAvailableSemaphores[i]);
        vkCreateSemaphore(device, &si, nullptr, &renderFinishedSemaphores[i]);
        vkCreateFence(device, &fi, nullptr, &inFlightFences[i]);
    }
}

void Renderer::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = (uint32_t)commandBuffers.size();
    vkAllocateCommandBuffers(device, &ai, commandBuffers.data());
}

void Renderer::createOffscreenResources() {
    offscreenExtent = {settings.offscreenWidth, settings.offscreenHeight};

    createImage(offscreenExtent.width, offscreenExtent.height, swapChainImageFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                offscreenColorImage, offscreenColorMemory);
    offscreenColorView = createImageView(offscreenColorImage, swapChainImageFormat);

    createImage(offscreenExtent.width, offscreenExtent.height, VK_FORMAT_D32_SFLOAT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                offscreenDepthImage, offscreenDepthMemory);
    offscreenDepthView = createImageView(offscreenDepthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

    transitionImageLayout(offscreenColorImage, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
    transitionImageLayout(offscreenDepthImage, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Renderer::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};
    bindings[3] = {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT};
    VkDescriptorSetLayoutCreateInfo li{};
    li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    li.bindingCount = (uint32_t)bindings.size();
    li.pBindings = bindings.data();
    vkCreateDescriptorSetLayout(device, &li, nullptr, &descriptorSetLayout);
}

void Renderer::createPipeline() {
    auto readFile = [](const std::string& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file) throw std::runtime_error("failed to open " + path);
        size_t size = file.tellg();
        std::vector<char> buf(size);
        file.seekg(0);
        file.read(buf.data(), size);
        return buf;
    };
    auto vertCode = readFile("shaders/vert.spv");
    auto fragCode = readFile("shaders/frag.spv");

    VkShaderModule vertMod, fragMod;
    VkShaderModuleCreateInfo smci{};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = vertCode.size();
    smci.pCode = (const uint32_t*)vertCode.data();
    vkCreateShaderModule(device, &smci, nullptr, &vertMod);
    smci.codeSize = fragCode.size();
    smci.pCode = (const uint32_t*)fragCode.data();
    vkCreateShaderModule(device, &smci, nullptr, &fragMod);

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    auto bindingDesc = Vertex::getBindingDescription();
    auto attrDescs = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bindingDesc;
    vi.vertexAttributeDescriptionCount = (uint32_t)attrDescs.size();
    vi.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{0,0,(float)offscreenExtent.width,(float)offscreenExtent.height,0,1};
    VkRect2D sc{{0,0},{offscreenExtent.width,offscreenExtent.height}};
    VkPipelineViewportStateCreateInfo vs{};
    vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vs.viewportCount = 1;
    vs.pViewports = &vp;
    vs.scissorCount = 1;
    vs.pScissors = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.0f;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pl{};
    pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &descriptorSetLayout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pcr;
    vkCreatePipelineLayout(device, &pl, nullptr, &pipelineLayout);

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &swapChainImageFormat;
    renderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    VkGraphicsPipelineCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pi.pNext = &renderingInfo;
    pi.stageCount = 2;
    pi.pStages = stages;
    pi.pVertexInputState = &vi;
    pi.pInputAssemblyState = &ia;
    pi.pViewportState = &vs;
    pi.pRasterizationState = &rs;
    pi.pMultisampleState = &ms;
    pi.pDepthStencilState = &ds;
    pi.pColorBlendState = &cb;
    pi.layout = pipelineLayout;

    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, nullptr, &graphicsPipeline);
    vkDestroyShaderModule(device, vertMod, nullptr);
    vkDestroyShaderModule(device, fragMod, nullptr);
}

void Renderer::createUniformBuffers() {
    VkDeviceSize size = sizeof(UniformBufferObject);
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffers[i], uniformBuffersMemory[i]);
        vkMapMemory(device, uniformBuffersMemory[i], 0, size, 0, &uniformBuffersMapped[i]);
    }
}

void Renderer::createDescriptorSets() {
    std::array<VkDescriptorPoolSize, 2> poolSizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 3},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT}
    };
    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.poolSizeCount = (uint32_t)poolSizes.size();
    pi.pPoolSizes = poolSizes.data();
    pi.maxSets = MAX_FRAMES_IN_FLIGHT;
    vkCreateDescriptorPool(device, &pi, nullptr, &descriptorPool);

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = descriptorPool;
    ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    ai.pSetLayouts = layouts.data();
    vkAllocateDescriptorSets(device, &ai, descriptorSets.data());

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorImageInfo globeInfo{};
        globeInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        globeInfo.imageView = textureView;
        globeInfo.sampler = textureSampler;

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo gridInfo{};
        gridInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        gridInfo.imageView = gridView;
        gridInfo.sampler = gridSampler;

        VkDescriptorImageInfo heightmapInfo{};
        heightmapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        heightmapInfo.imageView = heightmapView;
        heightmapInfo.sampler = heightmapSampler;

        std::array<VkWriteDescriptorSet, 4> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = descriptorSets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &globeInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = descriptorSets[i];
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &bufferInfo;

        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = descriptorSets[i];
        writes[2].dstBinding = 2;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].descriptorCount = 1;
        writes[2].pImageInfo = &gridInfo;

        writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet = descriptorSets[i];
        writes[3].dstBinding = 3;
        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[3].descriptorCount = 1;
        writes[3].pImageInfo = &heightmapInfo;

        vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
    }
}

void Renderer::updateUniforms(const Camera& camera, float displacementScale) {
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = camera.viewMatrix();
    ubo.proj = camera.projectionMatrix((float)swapChainExtent.width / (float)swapChainExtent.height);
    ubo.cameraPos = glm::vec4(camera.viewMatrix()[3]); // camera position from view matrix inverse? We'll just use camera's position
    ubo.displacementScale = displacementScale;
    memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, const Camera& camera, bool showGrid, bool showGridLines, float dispScale) {
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &bi);

    VkRenderingAttachmentInfo colorAtt{};
    colorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAtt.imageView = offscreenColorView;
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.clearValue = {0.35f, 0.55f, 0.75f, 1.0f};

    VkRenderingAttachmentInfo depthAtt{};
    depthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAtt.imageView = offscreenDepthView;
    depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAtt.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = {{0, 0}, offscreenExtent};
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAtt;
    renderInfo.pDepthAttachment = &depthAtt;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &sphereMesh.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, sphereMesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    PushConstants pc{};
    pc.gridOverlay = showGrid ? 1 : 0;
    pc.gridLineOverlay = showGridLines ? 1 : 0;
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

    vkCmdDrawIndexed(cmd, (uint32_t)sphereMesh.indices.size(), 1, 0, 0, 0);
    vkCmdEndRendering(cmd);

    // Transition offscreen color to transfer source, then blit to swapchain, transition back, present
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = offscreenColorImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    // Offscreen -> TRANSFER_SRC
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Swapchain image undefined -> TRANSFER_DST
    VkImageMemoryBarrier swapchainBarrier{};
    swapchainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainBarrier.image = swapChainImages[imageIndex];
    swapchainBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapchainBarrier.subresourceRange.levelCount = 1;
    swapchainBarrier.subresourceRange.layerCount = 1;
    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.srcAccessMask = 0;
    swapchainBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapchainBarrier);

    VkImageBlit blit{};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[1] = {(int32_t)offscreenExtent.width, (int32_t)offscreenExtent.height, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.layerCount = 1;
    blit.dstOffsets[1] = {(int32_t)swapChainExtent.width, (int32_t)swapChainExtent.height, 1};
    vkCmdBlitImage(cmd, offscreenColorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

    // Offscreen back to COLOR_ATTACHMENT
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Swapchain to PRESENT
    swapchainBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    swapchainBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapchainBarrier.dstAccessMask = 0;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapchainBarrier);

    vkEndCommandBuffer(cmd);
}

void Renderer::drawFrame(const Camera& camera, bool showGrid, bool showGridLines, float displacementScale) {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapChain(); return; }
    else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) throw std::runtime_error("acquire failed");

    updateUniforms(camera, displacementScale);

    vkResetFences(device, 1, &inFlightFences[currentFrame]);
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex, camera, showGrid, showGridLines, displacementScale);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
    submit.pWaitDstStageMask = waitStages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffers[currentFrame];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];
    vkQueueSubmit(graphicsQueue, 1, &submit, inFlightFences[currentFrame]);

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
    present.swapchainCount = 1;
    present.pSwapchains = &swapChain;
    present.pImageIndices = &imageIndex;
    res = vkQueuePresentKHR(presentQueue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || window->getInput().framebufferResized) {
        window->getInput().framebufferResized = false;
        recreateSwapChain();
    }
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ---------- Helper methods (copied from old project) ----------
uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("no suitable memory type");
}

void Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = usage;
    vkCreateBuffer(device, &bi, nullptr, &buffer);
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buffer, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);
    vkAllocateMemory(device, &ai, nullptr, &memory);
    vkBindBufferMemory(device, buffer, memory, 0);
}

void Renderer::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &ai, &cmd);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

void Renderer::createImage(uint32_t w, uint32_t h, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags props, VkImage& image, VkDeviceMemory& memory) {
    VkImageCreateInfo ii{};
    ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.extent = {w, h, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.format = format;
    ii.tiling = tiling;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage = usage;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    vkCreateImage(device, &ii, nullptr, &image);
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, image, &req);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);
    if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS) {
        ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device, &ai, nullptr, &memory);
    }
    vkBindImageMemory(device, image, memory, 0);
}

VkImageView Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) {
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange.aspectMask = aspect;
    vi.subresourceRange.levelCount = 1;
    vi.subresourceRange.layerCount = 1;
    VkImageView view;
    vkCreateImageView(device, &vi, nullptr, &view);
    return view;
}

void Renderer::transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &ai, &cmd);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) { barrier.srcAccessMask = 0; srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) { barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT; }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) { barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; }
    else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) { barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; srcStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }
    else throw std::runtime_error("unsupported old layout");

    if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT; }
    else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; }
    else if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; }
    else if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; }
    else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) { barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT; dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT; }
    else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) { barrier.dstAccessMask = 0; dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; }
    else throw std::runtime_error("unsupported new layout");

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

void Renderer::copyBufferToImageRegion(VkBuffer buffer, VkImage image, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    vkAllocateCommandBuffers(device, &ai, &cmd);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {(int32_t)x, (int32_t)y, 0};
    region.imageExtent = {w, h, 1};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

VkSampler Renderer::createSampler() {
    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.anisotropyEnable = VK_TRUE;
    si.maxAnisotropy = 4.0f;
    si.minLod = 0.0f;
    si.maxLod = 1.0f;
    VkSampler sampler;
    vkCreateSampler(device, &si, nullptr, &sampler);
    return sampler;
}

// --- Resource loading methods (using GDAL) ---
void Renderer::loadTextureFromFile(const std::string& path) {
    GDALAllRegister();
    GDALDataset* ds = (GDALDataset*)GDALOpen(path.c_str(), GA_ReadOnly);
    if (!ds) throw std::runtime_error("Failed to open texture: " + path);
    uint32_t w = ds->GetRasterXSize(), h = ds->GetRasterYSize();
    std::vector<uint8_t> rgb(w * h * 3);
    int bandMap[3] = {1,2,3};
    ds->RasterIO(GF_Read, 0, 0, w, h, rgb.data(), w, h, GDT_Byte, 3, bandMap, 3, w*3, 1, nullptr);
    GDALClose(ds);
    std::vector<uint8_t> rgba(w * h * 4);
    for (size_t i = 0; i < w*h; ++i) {
        rgba[i*4+0] = rgb[i*3+0]; rgba[i*4+1] = rgb[i*3+1];
        rgba[i*4+2] = rgb[i*3+2]; rgba[i*4+3] = 255;
    }

    createImage(w, h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureMemory);
    transitionImageLayout(textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    constexpr VkDeviceSize STAGING = 64*1024*1024;
    VkBuffer staging; VkDeviceMemory stagingMem;
    createBuffer(STAGING, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
    void* mapped;
    vkMapMemory(device, stagingMem, 0, STAGING, 0, &mapped);
    uint32_t maxRows = (uint32_t)(STAGING / (w*4));
    uint32_t y = 0;
    while (y < h) {
        uint32_t rows = std::min(maxRows, h - y);
        memcpy(mapped, rgba.data() + (size_t)y*w*4, (size_t)rows*w*4);
        copyBufferToImageRegion(staging, textureImage, 0, y, w, rows);
        y += rows;
    }
    vkUnmapMemory(device, stagingMem);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    transitionImageLayout(textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    textureView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM);
    textureSampler = createSampler();
}

void Renderer::loadHeightmapFromFile(const std::string& path) {
    GDALAllRegister();
    GDALDataset* ds = (GDALDataset*)GDALOpen(path.c_str(), GA_ReadOnly);
    if (!ds) throw std::runtime_error("Failed to open heightmap: " + path);
    uint32_t w = ds->GetRasterXSize(), h = ds->GetRasterYSize();
    std::vector<uint8_t> pixels(w * h);
    ds->RasterIO(GF_Read, 0, 0, w, h, pixels.data(), w, h, GDT_Byte, 1, nullptr, 0,0,0, nullptr);
    GDALClose(ds);

    createImage(w, h, VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, heightmapImage, heightmapMemory);
    transitionImageLayout(heightmapImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    constexpr VkDeviceSize STAGING = 64*1024*1024;
    VkBuffer staging; VkDeviceMemory stagingMem;
    createBuffer(STAGING, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
    void* mapped;
    vkMapMemory(device, stagingMem, 0, STAGING, 0, &mapped);
    uint32_t maxRows = (uint32_t)(STAGING / w);
    uint32_t y = 0;
    while (y < h) {
        uint32_t rows = std::min(maxRows, h - y);
        memcpy(mapped, pixels.data() + (size_t)y*w, (size_t)rows*w);
        copyBufferToImageRegion(staging, heightmapImage, 0, y, w, rows);
        y += rows;
    }
    vkUnmapMemory(device, stagingMem);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    transitionImageLayout(heightmapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    heightmapView = createImageView(heightmapImage, VK_FORMAT_R8_UNORM);
    heightmapSampler = createSampler();
}

void Renderer::loadGridFromFile(const std::string& path) {
    const size_t gridW = 32768, gridH = 32768, gridSize = gridW * gridH;
    std::vector<uint8_t> gridData(gridSize, 0);
    std::ifstream fin(path, std::ios::binary);
    if (fin) {
        fin.read((char*)gridData.data(), gridSize);
        size_t read = fin.gcount();
        if (read < gridSize) std::fill(gridData.begin() + read, gridData.end(), 0);
        std::cout << "📥 Loaded grid from " << path << "\n";
    } else {
        std::ofstream fout(path, std::ios::binary);
        std::vector<char> zeros(gridSize, 0);
        fout.write(zeros.data(), gridSize);
        std::cout << "📁 Created new grid file\n";
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    if (gridW > props.limits.maxImageDimension2D || gridH > props.limits.maxImageDimension2D)
        throw std::runtime_error("Grid texture too large for GPU");

    createImage((uint32_t)gridW, (uint32_t)gridH, VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gridImage, gridMemory);
    transitionImageLayout(gridImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    constexpr VkDeviceSize STAGING = 64*1024*1024;
    VkBuffer staging; VkDeviceMemory stagingMem;
    createBuffer(STAGING, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
    void* mapped;
    vkMapMemory(device, stagingMem, 0, STAGING, 0, &mapped);
    uint32_t maxRows = (uint32_t)(STAGING / gridW);
    uint32_t y = 0;
    while (y < gridH) {
        uint32_t rows = std::min(maxRows, (uint32_t)(gridH - y));
        memcpy(mapped, gridData.data() + y*gridW, rows*gridW);
        copyBufferToImageRegion(staging, gridImage, 0, y, (uint32_t)gridW, rows);
        y += rows;
    }
    vkUnmapMemory(device, stagingMem);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    transitionImageLayout(gridImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    gridView = createImageView(gridImage, VK_FORMAT_R8_UNORM);
    gridSampler = createSampler();
}