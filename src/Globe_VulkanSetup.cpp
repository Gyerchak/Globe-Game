#include "GlobeApp.h"
#include <set>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Helper: pick a memory type that supports the requested property flags
// (already declared in Globe_Helpers.cpp, but is used here by pickPhysicalDevice)
// That function is actually in Globe_Helpers.cpp – no need to duplicate.
// ---------------------------------------------------------------------------

void GlobeApp::createInstance() {
    VkApplicationInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = "GlobeViewer";
    ai.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    ai.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    ai.apiVersion = VK_API_VERSION_1_3;

    uint32_t glfwCnt;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwCnt);
    std::vector<const char*> exts(glfwExts, glfwExts + glfwCnt);

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &ai;
    ci.enabledExtensionCount = exts.size();
    ci.ppEnabledExtensionNames = exts.data();
    if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance!");
}

void GlobeApp::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create surface!");
}

GlobeApp::QueueFamilyIndices GlobeApp::findQueueFamilies(VkPhysicalDevice dev) {
    QueueFamilyIndices idx;
    uint32_t cnt;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &cnt, nullptr);
    std::vector<VkQueueFamilyProperties> qfs(cnt);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &cnt, qfs.data());
    for (uint32_t i = 0; i < cnt; ++i) {
        if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) idx.graphics = i;
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
        if (present) idx.present = i;
        if (idx.isComplete()) break;
    }
    return idx;
}

GlobeApp::SwapChainSupport GlobeApp::querySwapChainSupport(VkPhysicalDevice dev) {
    SwapChainSupport s;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &s.caps);
    uint32_t cnt;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &cnt, nullptr);
    if (cnt) {
        s.formats.resize(cnt);
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &cnt, s.formats.data());
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &cnt, nullptr);
    if (cnt) {
        s.modes.resize(cnt);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &cnt, s.modes.data());
    }
    return s;
}

bool GlobeApp::checkDeviceExtensionSupport(VkPhysicalDevice dev) {
    uint32_t cnt;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &cnt, nullptr);
    std::vector<VkExtensionProperties> avail(cnt);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &cnt, avail.data());
    std::set<std::string> required = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    for (auto& e : avail) required.erase(e.extensionName);
    return required.empty();
}

bool GlobeApp::isDeviceSuitable(VkPhysicalDevice dev) {
    QueueFamilyIndices idx = findQueueFamilies(dev);
    bool extOk = checkDeviceExtensionSupport(dev);
    bool swapOk = false;
    if (extOk) {
        auto ss = querySwapChainSupport(dev);
        swapOk = !ss.formats.empty() && !ss.modes.empty();
    }
    return idx.isComplete() && extOk && swapOk;
}

void GlobeApp::pickPhysicalDevice() {
    uint32_t cnt;
    vkEnumeratePhysicalDevices(instance, &cnt, nullptr);
    if (!cnt) throw std::runtime_error("no Vulkan GPUs");
    std::vector<VkPhysicalDevice> devs(cnt);
    vkEnumeratePhysicalDevices(instance, &cnt, devs.data());
    for (auto& d : devs) {
        if (isDeviceSuitable(d)) { physicalDevice = d; break; }
    }
    if (!physicalDevice) throw std::runtime_error("no suitable GPU");
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "🖥️  GPU: " << props.deviceName << "\n";
    std::cout << "   Max texture size: " << props.limits.maxImageDimension2D << "\n";
}

void GlobeApp::createLogicalDevice() {
    auto idx = findQueueFamilies(physicalDevice);
    std::set<uint32_t> uniq = {idx.graphics.value(), idx.present.value()};
    std::vector<VkDeviceQueueCreateInfo> qcis;
    float prio = 1.0f;
    for (uint32_t fam : uniq) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = fam;
        qi.queueCount = 1;
        qi.pQueuePriorities = &prio;
        qcis.push_back(qi);
    }
    VkPhysicalDeviceFeatures feats{};
    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = qcis.size();
    dci.pQueueCreateInfos = qcis.data();
    dci.pEnabledFeatures = &feats;
    const char* swapExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = &swapExt;
    dci.enabledLayerCount = 0;
    if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("failed to create logical device!");
    vkGetDeviceQueue(device, idx.graphics.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, idx.present.value(), 0, &presentQueue);
}