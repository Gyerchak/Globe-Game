// squaremapglobe.cpp – fixed version (simple shader, RGBA texture)
// Compile: g++ -std=c++20 -O3 src/squaremapglobe.cpp -o exe/squaremapglobe -lgdal -lvulkan -lglfw
// Run: ./exe/squaremapglobe

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <gdal_priv.h>
#include <gdal_utils.h>

#include <iostream>
#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <optional>
#include <set>
#include <chrono>
#include <algorithm>
#include <cmath>

constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;
constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex {
    glm::vec3 pos;
    glm::vec2 texCoord;
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription b{};
        b.binding = 0;
        b.stride = sizeof(Vertex);
        b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return b;
    }
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> a{};
        a[0].binding = 0;
        a[0].location = 0;
        a[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        a[0].offset = offsetof(Vertex, pos);
        a[1].binding = 0;
        a[1].location = 1;
        a[1].format = VK_FORMAT_R32G32_SFLOAT;
        a[1].offset = offsetof(Vertex, texCoord);
        return a;
    }
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos;
};

struct PushConstants { int waterOverlay; };

class GlobeApp {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE, presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkBuffer vertexBuffer = VK_NULL_HANDLE, indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE, indexBufferMemory = VK_NULL_HANDLE;

    // Texture (single RGBA image)
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkSemaphore> imageAvailableSemaphores, renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    float camDistance = 2.5f, camPitch = 0.0f, camYaw = 0.0f, camRoll = 0.0f;
    glm::vec3 camTarget = glm::vec3(0.0f);
    bool waterOverlay = false;
    glm::vec2 lastMousePos;
    bool rightMouseDown = false;
    bool keys[512] = {};
    float deltaTime = 0.0f;

    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    static void keyCallback(GLFWwindow* w, int key, int, int action, int) {
        auto* app = (GlobeApp*)glfwGetWindowUserPointer(w);
        if (action == GLFW_PRESS) {
            if (key == GLFW_KEY_V) app->waterOverlay = !app->waterOverlay;
            if (key == GLFW_KEY_R) { app->camPitch = app->camYaw = app->camRoll = 0.0f; app->camDistance = 2.5f; }
            if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
        }
        if (key >= 0 && key < 512) app->keys[key] = (action == GLFW_PRESS || action == GLFW_REPEAT);
    }
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
        auto* app = (GlobeApp*)glfwGetWindowUserPointer(w);
        if (button == GLFW_MOUSE_BUTTON_RIGHT) app->rightMouseDown = (action == GLFW_PRESS);
        double x, y; glfwGetCursorPos(w, &x, &y);
        app->lastMousePos = glm::vec2(x, y);
    }
    static void cursorPosCallback(GLFWwindow* w, double xpos, double ypos) {
        auto* app = (GlobeApp*)glfwGetWindowUserPointer(w);
        glm::vec2 newPos(xpos, ypos);
        glm::vec2 delta = newPos - app->lastMousePos;
        app->lastMousePos = newPos;
        if (app->rightMouseDown) {
            app->camYaw += delta.x * 0.005f;
            app->camPitch -= delta.y * 0.005f;
        }
    }
    static void scrollCallback(GLFWwindow* w, double, double yoffset) {
        auto* app = (GlobeApp*)glfwGetWindowUserPointer(w);
        app->camDistance *= (1.0f - (float)yoffset * 0.15f);
        app->camDistance = glm::clamp(app->camDistance, 1.08f, 5000.0f);
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
            if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;
        throw std::runtime_error("failed to find suitable memory type!");
    }

    std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file) throw std::runtime_error("failed to open " + filename);
        size_t size = file.tellg();
        std::vector<char> buf(size);
        file.seekg(0);
        file.read(buf.data(), size);
        return buf;
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = code.size();
        ci.pCode = (const uint32_t*)code.data();
        VkShaderModule mod;
        if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
            throw std::runtime_error("failed to create shader module!");
        return mod;
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                      VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = size;
        bi.usage = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bi, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create buffer!");
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, buffer, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);
        if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate buffer memory!");
        vkBindBufferMemory(device, buffer, memory, 0);
                      }

                      void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
                          VkCommandBufferAllocateInfo ai{};
                          ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                          ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                          ai.commandPool = commandPool;
                          ai.commandBufferCount = 1;
                          VkCommandBuffer cmd;
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

                      void createImage(uint32_t w, uint32_t h, VkFormat format, VkImageTiling tiling,
                                       VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                                       VkImage& image, VkDeviceMemory& memory) {
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
                          ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                          if (vkCreateImage(device, &ii, nullptr, &image) != VK_SUCCESS)
                              throw std::runtime_error("failed to create image!");
                          VkMemoryRequirements req;
                          vkGetImageMemoryRequirements(device, image, &req);
                          std::cout << "📦 Image memory requirement: " << req.size / (1024*1024) << " MB\n";
                          VkMemoryAllocateInfo ai{};
                          ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                          ai.allocationSize = req.size;
                          ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);
                          if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS) {
                              std::cerr << "⚠️  Device-local allocation failed, trying host-visible...\n";
                              ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits,
                                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
                              if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS)
                                  throw std::runtime_error("failed to allocate image memory (even with fallback)!");
                          }
                          vkBindImageMemory(device, image, memory, 0);
                                       }

                                       VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
                                           VkImageViewCreateInfo vi{};
                                           vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                                           vi.image = image;
                                           vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
                                           vi.format = format;
                                           vi.subresourceRange.aspectMask = aspect;
                                           vi.subresourceRange.baseMipLevel = 0;
                                           vi.subresourceRange.levelCount = 1;
                                           vi.subresourceRange.baseArrayLayer = 0;
                                           vi.subresourceRange.layerCount = 1;
                                           VkImageView view;
                                           if (vkCreateImageView(device, &vi, nullptr, &view) != VK_SUCCESS)
                                               throw std::runtime_error("failed to create image view!");
                                           return view;
                                       }

                                       void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
                                           VkCommandBufferAllocateInfo ai{};
                                           ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                                           ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                                           ai.commandPool = commandPool;
                                           ai.commandBufferCount = 1;
                                           VkCommandBuffer cmd;
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
                                           barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                                           barrier.subresourceRange.baseMipLevel = 0;
                                           barrier.subresourceRange.levelCount = 1;
                                           barrier.subresourceRange.baseArrayLayer = 0;
                                           barrier.subresourceRange.layerCount = 1;
                                           VkPipelineStageFlags srcStage, dstStage;
                                           if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                                               barrier.srcAccessMask = 0;
                                               barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                                               srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                                               dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                                           } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                                               barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                                               barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                                               srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                                               dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                                           } else {
                                               throw std::runtime_error("unsupported layout transition");
                                           }
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

                                       void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t w, uint32_t h) {
                                           VkCommandBufferAllocateInfo ai{};
                                           ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                                           ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                                           ai.commandPool = commandPool;
                                           ai.commandBufferCount = 1;
                                           VkCommandBuffer cmd;
                                           vkAllocateCommandBuffers(device, &ai, &cmd);
                                           VkCommandBufferBeginInfo bi{};
                                           bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                                           bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                                           vkBeginCommandBuffer(cmd, &bi);
                                           VkBufferImageCopy region{};
                                           region.bufferOffset = 0;
                                           region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                                           region.imageSubresource.mipLevel = 0;
                                           region.imageSubresource.baseArrayLayer = 0;
                                           region.imageSubresource.layerCount = 1;
                                           region.imageOffset = {0,0,0};
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

                                       VkSampler createSampler() {
                                           VkSamplerCreateInfo si{};
                                           si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                                           si.magFilter = VK_FILTER_LINEAR;
                                           si.minFilter = VK_FILTER_LINEAR;
                                           si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                                           si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                                           si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                                           si.anisotropyEnable = VK_TRUE;
                                           si.maxAnisotropy = 4.0f;
                                           si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
                                           si.unnormalizedCoordinates = VK_FALSE;
                                           si.compareEnable = VK_FALSE;
                                           si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                                           si.minLod = 0.0f;
                                           si.maxLod = 1.0f;
                                           VkSampler sampler;
                                           if (vkCreateSampler(device, &si, nullptr, &sampler) != VK_SUCCESS)
                                               throw std::runtime_error("failed to create sampler!");
                                           return sampler;
                                       }

                                       void initWindow() {
                                           glfwInit();
                                           glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                                           glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
                                           window = glfwCreateWindow(WIDTH, HEIGHT, "Globe Viewer", nullptr, nullptr);
                                           glfwSetWindowUserPointer(window, this);
                                           glfwSetKeyCallback(window, keyCallback);
                                           glfwSetMouseButtonCallback(window, mouseButtonCallback);
                                           glfwSetCursorPosCallback(window, cursorPosCallback);
                                           glfwSetScrollCallback(window, scrollCallback);
                                       }

                                       void initVulkan() {
                                           createInstance();
                                           createSurface();
                                           pickPhysicalDevice();
                                           createLogicalDevice();
                                           createSwapChain();
                                           createImageViews();
                                           createDepthResources();
                                           createRenderPass();
                                           createDescriptorSetLayout();
                                           createGraphicsPipeline();
                                           createFramebuffers();
                                           createCommandPool();
                                           prepareSphere();
                                           loadTexture();
                                           createUniformBuffers();
                                           createDescriptorSets();
                                           createCommandBuffers();
                                           createSyncObjects();
                                       }

                                       void createInstance() {
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

                                       void createSurface() {
                                           if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
                                               throw std::runtime_error("failed to create surface!");
                                       }

                                       struct QueueFamilyIndices {
                                           std::optional<uint32_t> graphics, present;
                                           bool isComplete() { return graphics && present; }
                                       };
                                       QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) {
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

                                       struct SwapChainSupport {
                                           VkSurfaceCapabilitiesKHR caps;
                                           std::vector<VkSurfaceFormatKHR> formats;
                                           std::vector<VkPresentModeKHR> modes;
                                       };
                                       SwapChainSupport querySwapChainSupport(VkPhysicalDevice dev) {
                                           SwapChainSupport s;
                                           vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &s.caps);
                                           uint32_t cnt;
                                           vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &cnt, nullptr);
                                           if (cnt) { s.formats.resize(cnt); vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &cnt, s.formats.data()); }
                                           vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &cnt, nullptr);
                                           if (cnt) { s.modes.resize(cnt); vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &cnt, s.modes.data()); }
                                           return s;
                                       }

                                       void pickPhysicalDevice() {
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

                                       bool isDeviceSuitable(VkPhysicalDevice dev) {
                                           QueueFamilyIndices idx = findQueueFamilies(dev);
                                           bool extOk = checkDeviceExtensionSupport(dev);
                                           bool swapOk = false;
                                           if (extOk) {
                                               auto ss = querySwapChainSupport(dev);
                                               swapOk = !ss.formats.empty() && !ss.modes.empty();
                                           }
                                           return idx.isComplete() && extOk && swapOk;
                                       }

                                       bool checkDeviceExtensionSupport(VkPhysicalDevice dev) {
                                           uint32_t cnt;
                                           vkEnumerateDeviceExtensionProperties(dev, nullptr, &cnt, nullptr);
                                           std::vector<VkExtensionProperties> avail(cnt);
                                           vkEnumerateDeviceExtensionProperties(dev, nullptr, &cnt, avail.data());
                                           std::set<std::string> required = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
                                           for (auto& e : avail) required.erase(e.extensionName);
                                           return required.empty();
                                       }

                                       void createLogicalDevice() {
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

                                       void createSwapChain() {
                                           auto ss = querySwapChainSupport(physicalDevice);
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

                                       VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& avail) {
                                           for (auto& f : avail) if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
                                           return avail[0];
                                       }
                                       VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& avail) {
                                           for (auto& m : avail) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
                                           return VK_PRESENT_MODE_FIFO_KHR;
                                       }
                                       VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps) {
                                           if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent;
                                           int w, h; glfwGetFramebufferSize(window, &w, &h);
                                           return { (uint32_t)w, (uint32_t)h };
                                       }

                                       void createImageViews() {
                                           swapChainImageViews.resize(swapChainImages.size());
                                           for (size_t i = 0; i < swapChainImages.size(); ++i)
                                               swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat);
                                       }

                                       void createDepthResources() {
                                           VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
                                           createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                                                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                       depthImage, depthImageMemory);
                                           depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
                                       }

                                       void createRenderPass() {
                                           VkAttachmentDescription color{}, depth{};
                                           color.format = swapChainImageFormat;
                                           color.samples = VK_SAMPLE_COUNT_1_BIT;
                                           color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                                           color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                                           color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                                           color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                                           depth.format = VK_FORMAT_D32_SFLOAT;
                                           depth.samples = VK_SAMPLE_COUNT_1_BIT;
                                           depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                                           depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                                           depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                                           depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                                           VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
                                           VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
                                           VkSubpassDescription subpass{};
                                           subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                                           subpass.colorAttachmentCount = 1;
                                           subpass.pColorAttachments = &colorRef;
                                           subpass.pDepthStencilAttachment = &depthRef;
                                           std::array<VkAttachmentDescription, 2> atts = {color, depth};
                                           VkRenderPassCreateInfo rpi{};
                                           rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
                                           rpi.attachmentCount = atts.size();
                                           rpi.pAttachments = atts.data();
                                           rpi.subpassCount = 1;
                                           rpi.pSubpasses = &subpass;
                                           if (vkCreateRenderPass(device, &rpi, nullptr, &renderPass) != VK_SUCCESS)
                                               throw std::runtime_error("failed to create render pass!");
                                       }

                                       void createDescriptorSetLayout() {
                                           std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
                                           bindings[0].binding = 0;
                                           bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                                           bindings[0].descriptorCount = 1;
                                           bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
                                           bindings[1].binding = 1;
                                           bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                                           bindings[1].descriptorCount = 1;
                                           bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                                           VkDescriptorSetLayoutCreateInfo li{};
                                           li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                                           li.bindingCount = bindings.size();
                                           li.pBindings = bindings.data();
                                           if (vkCreateDescriptorSetLayout(device, &li, nullptr, &descriptorSetLayout) != VK_SUCCESS)
                                               throw std::runtime_error("failed to create descriptor set layout!");
                                       }

                                       void createGraphicsPipeline() {
                                           auto vertCode = readFile("shaders/vert.spv");
                                           auto fragCode = readFile("shaders/frag.spv");
                                           VkShaderModule vertMod = createShaderModule(vertCode);
                                           VkShaderModule fragMod = createShaderModule(fragCode);
                                           VkPipelineShaderStageCreateInfo vertStage{}, fragStage{};
                                           vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                                           vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
                                           vertStage.module = vertMod;
                                           vertStage.pName = "main";
                                           fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                                           fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                                           fragStage.module = fragMod;
                                           fragStage.pName = "main";
                                           VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

                                           auto bd = Vertex::getBindingDescription();
                                           auto ads = Vertex::getAttributeDescriptions();
                                           VkPipelineVertexInputStateCreateInfo vi{};
                                           vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
                                           vi.vertexBindingDescriptionCount = 1;
                                           vi.pVertexBindingDescriptions = &bd;
                                           vi.vertexAttributeDescriptionCount = ads.size();
                                           vi.pVertexAttributeDescriptions = ads.data();

                                           VkPipelineInputAssemblyStateCreateInfo ia{};
                                           ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
                                           ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
                                           ia.primitiveRestartEnable = VK_FALSE;

                                           VkViewport vp{0,0,(float)swapChainExtent.width,(float)swapChainExtent.height,0,1};
                                           VkRect2D sc{{0,0}, swapChainExtent};
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
                                           cba.blendEnable = VK_FALSE;
                                           VkPipelineColorBlendStateCreateInfo cb{};
                                           cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
                                           cb.attachmentCount = 1;
                                           cb.pAttachments = &cba;

                                           VkPushConstantRange pcr{};
                                           pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
                                           pcr.offset = 0;
                                           pcr.size = sizeof(PushConstants);

                                           VkPipelineLayoutCreateInfo pl{};
                                           pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                                           pl.setLayoutCount = 1;
                                           pl.pSetLayouts = &descriptorSetLayout;
                                           pl.pushConstantRangeCount = 1;
                                           pl.pPushConstantRanges = &pcr;
                                           if (vkCreatePipelineLayout(device, &pl, nullptr, &pipelineLayout) != VK_SUCCESS)
                                               throw std::runtime_error("failed to create pipeline layout!");

                                           VkGraphicsPipelineCreateInfo pi{};
                                           pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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
                                           pi.renderPass = renderPass;
                                           pi.subpass = 0;
                                           if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, nullptr, &graphicsPipeline) != VK_SUCCESS)
                                               throw std::runtime_error("failed to create graphics pipeline!");
                                           vkDestroyShaderModule(device, vertMod, nullptr);
                                           vkDestroyShaderModule(device, fragMod, nullptr);
                                       }

                                       void createFramebuffers() {
                                           swapChainFramebuffers.resize(swapChainImageViews.size());
                                           for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
                                               VkImageView attachments[] = {swapChainImageViews[i], depthImageView};
                                               VkFramebufferCreateInfo fi{};
                                               fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                                               fi.renderPass = renderPass;
                                               fi.attachmentCount = 2;
                                               fi.pAttachments = attachments;
                                               fi.width = swapChainExtent.width;
                                               fi.height = swapChainExtent.height;
                                               fi.layers = 1;
                                               if (vkCreateFramebuffer(device, &fi, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
                                                   throw std::runtime_error("failed to create framebuffer!");
                                           }
                                       }

                                       void createCommandPool() {
                                           auto idx = findQueueFamilies(physicalDevice);
                                           VkCommandPoolCreateInfo pi{};
                                           pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                                           pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                                           pi.queueFamilyIndex = idx.graphics.value();
                                           if (vkCreateCommandPool(device, &pi, nullptr, &commandPool) != VK_SUCCESS)
                                               throw std::runtime_error("failed to create command pool!");
                                       }

                                       void prepareSphere() {
                                           constexpr size_t lonSegs = 512, latSegs = 256;
                                           vertices.clear();
                                           indices.clear();
                                           for (size_t j = 0; j <= latSegs; ++j) {
                                               float theta = (float)j * glm::pi<float>() / (float)latSegs;
                                               float y = glm::cos(theta), sinTheta = glm::sin(theta);
                                               for (size_t i = 0; i <= lonSegs; ++i) {
                                                   float phi = (float)i * 2.0f * glm::pi<float>() / (float)lonSegs;
                                                   float x = glm::cos(phi) * sinTheta;
                                                   float z = glm::sin(phi) * sinTheta;
                                                   vertices.push_back({{x,y,z}, {(float)i / (float)lonSegs, (float)j / (float)latSegs}});
                                               }
                                           }
                                           for (size_t j = 0; j < latSegs; ++j) {
                                               for (size_t i = 0; i < lonSegs; ++i) {
                                                   uint32_t first = (uint32_t)(j * (lonSegs + 1) + i);
                                                   uint32_t second = first + (uint32_t)(lonSegs + 1);
                                                   indices.push_back(first);
                                                   indices.push_back(second);
                                                   indices.push_back(first + 1);
                                                   indices.push_back(second);
                                                   indices.push_back(second + 1);
                                                   indices.push_back(first + 1);
                                               }
                                           }
                                           VkDeviceSize vbSize = sizeof(Vertex) * vertices.size();
                                           VkBuffer staging; VkDeviceMemory stagingMem;
                                           createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                        staging, stagingMem);
                                           void* data; vkMapMemory(device, stagingMem, 0, vbSize, 0, &data);
                                           memcpy(data, vertices.data(), vbSize); vkUnmapMemory(device, stagingMem);
                                           createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
                                           copyBuffer(staging, vertexBuffer, vbSize);
                                           vkDestroyBuffer(device, staging, nullptr); vkFreeMemory(device, stagingMem, nullptr);
                                           VkDeviceSize ibSize = sizeof(uint32_t) * indices.size();
                                           createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                        staging, stagingMem);
                                           vkMapMemory(device, stagingMem, 0, ibSize, 0, &data);
                                           memcpy(data, indices.data(), ibSize); vkUnmapMemory(device, stagingMem);
                                           createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
                                           copyBuffer(staging, indexBuffer, ibSize);
                                           vkDestroyBuffer(device, staging, nullptr); vkFreeMemory(device, stagingMem, nullptr);
                                       }

                                       void loadTexture() {
                                           std::string fname = "input/squarecolormap.tif";
                                           GDALAllRegister();
                                           GDALDataset* ds = (GDALDataset*)GDALOpen(fname.c_str(), GA_ReadOnly);
                                           if (!ds) throw std::runtime_error("Failed to open " + fname);
                                           int w = ds->GetRasterXSize();
                                           int h = ds->GetRasterYSize();
                                           std::cout << "📂 Texture: " << w << " x " << h << "\n";

                                           // Read as RGBA
                                           const int channels = 4;
                                           size_t pixelBytes = w * h * channels;
                                           std::vector<uint8_t> pixels(pixelBytes);
                                           std::vector<uint8_t> rgb(w * h * 3);
                                           CPLErr err = ds->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, w, h,
                                                                                       rgb.data(), w, h, GDT_Byte, 3, 0);
                                           GDALClose(ds);
                                           if (err != CE_None) throw std::runtime_error("Failed to read texture pixels.");

                                           // Expand RGB to RGBA
                                           for (int i = 0; i < w * h; ++i) {
                                               pixels[i*4 + 0] = rgb[i*3 + 0];
                                               pixels[i*4 + 1] = rgb[i*3 + 1];
                                               pixels[i*4 + 2] = rgb[i*3 + 2];
                                               pixels[i*4 + 3] = 255;
                                           }

                                           VkDeviceSize size = pixelBytes;
                                           VkBuffer staging; VkDeviceMemory stagingMem;
                                           createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                        staging, stagingMem);
                                           void* data; vkMapMemory(device, stagingMem, 0, size, 0, &data);
                                           memcpy(data, pixels.data(), size);
                                           vkUnmapMemory(device, stagingMem);

                                           createImage((uint32_t)w, (uint32_t)h, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
                                           transitionImageLayout(textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                                           copyBufferToImage(staging, textureImage, w, h);
                                           transitionImageLayout(textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                                           vkDestroyBuffer(device, staging, nullptr);
                                           vkFreeMemory(device, stagingMem, nullptr);

                                           textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM);
                                           textureSampler = createSampler();
                                       }

                                       void createUniformBuffers() {
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

                                       void createDescriptorSets() {
                                           std::array<VkDescriptorPoolSize, 2> poolSizes = {
                                               VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT},
                                               VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT}
                                           };
                                           VkDescriptorPoolCreateInfo pi{};
                                           pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                                           pi.poolSizeCount = poolSizes.size();
                                           pi.pPoolSizes = poolSizes.data();
                                           pi.maxSets = MAX_FRAMES_IN_FLIGHT;
                                           if (vkCreateDescriptorPool(device, &pi, nullptr, &descriptorPool) != VK_SUCCESS)
                                               throw std::runtime_error("failed to create descriptor pool!");

                                           descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
                                           VkDescriptorSetAllocateInfo ai{};
                                           ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                                           ai.descriptorPool = descriptorPool;
                                           ai.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
                                           std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
                                           ai.pSetLayouts = layouts.data();
                                           if (vkAllocateDescriptorSets(device, &ai, descriptorSets.data()) != VK_SUCCESS)
                                               throw std::runtime_error("failed to allocate descriptor sets!");

                                           for (size_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
                                               VkDescriptorImageInfo imageInfo{};
                                               imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                                               imageInfo.imageView = textureImageView;
                                               imageInfo.sampler = textureSampler;

                                               VkDescriptorBufferInfo bufferInfo{};
                                               bufferInfo.buffer = uniformBuffers[f];
                                               bufferInfo.offset = 0;
                                               bufferInfo.range = sizeof(UniformBufferObject);

                                               std::array<VkWriteDescriptorSet, 2> writes{};
                                               writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                                               writes[0].dstSet = descriptorSets[f];
                                               writes[0].dstBinding = 0;
                                               writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                                               writes[0].descriptorCount = 1;
                                               writes[0].pImageInfo = &imageInfo;

                                               writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                                               writes[1].dstSet = descriptorSets[f];
                                               writes[1].dstBinding = 1;
                                               writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                                               writes[1].descriptorCount = 1;
                                               writes[1].pBufferInfo = &bufferInfo;

                                               vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
                                           }
                                       }

                                       void createCommandBuffers() {
                                           commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
                                           VkCommandBufferAllocateInfo ai{};
                                           ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                                           ai.commandPool = commandPool;
                                           ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                                           ai.commandBufferCount = (uint32_t)commandBuffers.size();
                                           if (vkAllocateCommandBuffers(device, &ai, commandBuffers.data()) != VK_SUCCESS)
                                               throw std::runtime_error("failed to allocate command buffers!");
                                       }

                                       void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
                                           VkCommandBufferBeginInfo bi{};
                                           bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                                           vkBeginCommandBuffer(cmd, &bi);

                                           VkRenderPassBeginInfo rpi{};
                                           rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                                           rpi.renderPass = renderPass;
                                           rpi.framebuffer = swapChainFramebuffers[imageIndex];
                                           rpi.renderArea = {{0,0}, swapChainExtent};
                                           VkClearValue colorClear{0.35f,0.55f,0.75f,1.0f};
                                           VkClearValue depthClear{1.0f,0};
                                           std::array<VkClearValue,2> clears = {colorClear, depthClear};
                                           rpi.clearValueCount = 2;
                                           rpi.pClearValues = clears.data();

                                           vkCmdBeginRenderPass(cmd, &rpi, VK_SUBPASS_CONTENTS_INLINE);
                                           vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

                                           VkDeviceSize offsets[] = {0};
                                           vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
                                           vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                                           vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

                                           PushConstants pc{ waterOverlay ? 1 : 0 };
                                           vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

                                           vkCmdDrawIndexed(cmd, (uint32_t)indices.size(), 1, 0, 0, 0);
                                           vkCmdEndRenderPass(cmd);
                                           vkEndCommandBuffer(cmd);
                                       }

                                       void createSyncObjects() {
                                           imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
                                           renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
                                           inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
                                           VkSemaphoreCreateInfo si{};
                                           si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                                           VkFenceCreateInfo fi{};
                                           fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                                           fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                                           for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                                               if (vkCreateSemaphore(device, &si, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                                                   vkCreateSemaphore(device, &si, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                                                   vkCreateFence(device, &fi, nullptr, &inFlightFences[i]) != VK_SUCCESS)
                                                   throw std::runtime_error("failed to create sync objects!");
                                           }
                                       }

                                       void mainLoop() {
                                           auto lastTime = std::chrono::high_resolution_clock::now();
                                           while (!glfwWindowShouldClose(window)) {
                                               auto cur = std::chrono::high_resolution_clock::now();
                                               deltaTime = std::chrono::duration<float>(cur - lastTime).count();
                                               lastTime = cur;
                                               glfwPollEvents();
                                               updateCamera();
                                               drawFrame();
                                           }
                                           vkDeviceWaitIdle(device);
                                       }

                                       void updateCamera() {
                                           float speed = 0.8f * deltaTime;
                                           if (keys[GLFW_KEY_LEFT_SHIFT] || keys[GLFW_KEY_RIGHT_SHIFT]) speed *= 3.0f;
                                           if (keys[GLFW_KEY_W]) camPitch += speed;
                                           if (keys[GLFW_KEY_S]) camPitch -= speed;
                                           if (keys[GLFW_KEY_A]) camYaw -= speed;
                                           if (keys[GLFW_KEY_D]) camYaw += speed;
                                           if (keys[GLFW_KEY_Q]) camRoll += speed;
                                           if (keys[GLFW_KEY_E]) camRoll -= speed;
                                           if (keys[GLFW_KEY_Z]) camDistance *= (1.0f - speed * 0.5f);
                                           if (keys[GLFW_KEY_X]) camDistance *= (1.0f + speed * 0.5f);
                                           camDistance = glm::clamp(camDistance, 1.08f, 5000.0f);

                                           float cp = glm::cos(camPitch), sp = glm::sin(camPitch);
                                           float cy = glm::cos(camYaw), sy = glm::sin(camYaw);
                                           glm::vec3 camPos;
                                           camPos.x = camDistance * cp * sy;
                                           camPos.y = camDistance * sp;
                                           camPos.z = camDistance * cp * cy;

                                           glm::vec3 up(-sp * sy, cp, -sp * cy);
                                           glm::mat4 view = glm::lookAt(camPos, camTarget, up);
                                           glm::vec3 forward = glm::normalize(camTarget - camPos);
                                           view = glm::rotate(view, camRoll, forward);

                                           float aspect = (float)swapChainExtent.width / (float)swapChainExtent.height;
                                           float nearPlane = glm::max(0.001f, camDistance * 0.0005f);
                                           float farPlane = glm::max(100.0f, camDistance * 5.0f);
                                           glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, nearPlane, farPlane);
                                           proj[1][1] *= -1;
                                           proj[2][2] = proj[2][2] * 0.5f + proj[3][2] * 0.5f;
                                           proj[2][3] = proj[2][3] * 0.5f + proj[3][3] * 0.5f;

                                           UniformBufferObject ubo{};
                                           ubo.model = glm::mat4(1.0f);
                                           ubo.view = view;
                                           ubo.proj = proj;
                                           ubo.cameraPos = glm::vec4(camPos, 0.0f);
                                           memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
                                       }

                                       void drawFrame() {
                                           vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
                                           uint32_t imageIndex;
                                           vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
                                           vkResetFences(device, 1, &inFlightFences[currentFrame]);
                                           vkResetCommandBuffer(commandBuffers[currentFrame], 0);
                                           recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

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
                                           vkQueuePresentKHR(presentQueue, &present);
                                           currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
                                       }

                                       void cleanup() {
                                           if (device == VK_NULL_HANDLE) return;
                                           vkDeviceWaitIdle(device);
                                           for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                                               if (renderFinishedSemaphores[i]) vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
                                               if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
                                               if (inFlightFences[i]) vkDestroyFence(device, inFlightFences[i], nullptr);
                                           }
                                           if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
                                           for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                                               if (uniformBuffers[i]) vkDestroyBuffer(device, uniformBuffers[i], nullptr);
                                               if (uniformBuffersMemory[i]) vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
                                           }
                                           if (textureSampler) vkDestroySampler(device, textureSampler, nullptr);
                                           if (textureImageView) vkDestroyImageView(device, textureImageView, nullptr);
                                           if (textureImage) vkDestroyImage(device, textureImage, nullptr);
                                           if (textureImageMemory) vkFreeMemory(device, textureImageMemory, nullptr);
                                           if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
                                           if (graphicsPipeline) vkDestroyPipeline(device, graphicsPipeline, nullptr);
                                           if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
                                           if (descriptorSetLayout) vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
                                           if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
                                           for (auto& fb : swapChainFramebuffers) if (fb) vkDestroyFramebuffer(device, fb, nullptr);
                                           if (depthImageView) vkDestroyImageView(device, depthImageView, nullptr);
                                           if (depthImage) vkDestroyImage(device, depthImage, nullptr);
                                           if (depthImageMemory) vkFreeMemory(device, depthImageMemory, nullptr);
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
};

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
