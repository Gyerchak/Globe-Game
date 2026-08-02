#pragma once

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
    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
};

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos;
};

struct PushConstants { 
    int gridOverlay;
    int gridOverlay;
};

class GlobeApp {
public:
    void run();

private:
    // Window
    GLFWwindow* window = nullptr;
    bool isFullscreen = false;
    GLFWmonitor* monitor = nullptr;
    int windowedX = 0, windowedY = 0, windowedW = WIDTH, windowedH = HEIGHT;
    bool keys[512] = {};
    bool rightMouseDown = false, middleMouseDown = false, shiftDown = false;
    glm::vec2 lastMousePos;
    void initWindow();
    void toggleFullscreen();
    static void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* w, double xoffset, double yoffset);

    // Vulkan core
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE, presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics, present;
        bool isComplete() { return graphics && present; }
    };
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev);
    bool checkDeviceExtensionSupport(VkPhysicalDevice dev);
    bool isDeviceSuitable(VkPhysicalDevice dev);

    // Swap chain support
    struct SwapChainSupport {
        VkSurfaceCapabilitiesKHR caps;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> modes;
    };
    SwapChainSupport querySwapChainSupport(VkPhysicalDevice dev);

    // Swap chain
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    void createSwapChain();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& avail);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& avail);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& caps);
    void createImageViews();
    void createDepthResources();
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    // Pipeline
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();          // <-- added
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    // Mesh
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkBuffer vertexBuffer = VK_NULL_HANDLE, indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE, indexBufferMemory = VK_NULL_HANDLE;
    void prepareSphere();

    // Textures
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;
    VkImageView textureImageView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;
    void loadTexture();

    // Grid overlay
    std::vector<uint8_t> gridData;
    VkImage gridImage = VK_NULL_HANDLE;
    VkDeviceMemory gridImageMemory = VK_NULL_HANDLE;
    VkImageView gridImageView = VK_NULL_HANDLE;
    VkSampler gridSampler = VK_NULL_HANDLE;
    bool showGrid = false;
    bool showGridLines = false;
    void loadGrid();
    void saveGridToFile();

    // Uniforms & descriptors
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;
    void createUniformBuffers();
    void createDescriptorSets();

    // Synchronisation & drawing
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores, renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    void createCommandPool();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void createSyncObjects();
    void drawFrame();

    // Camera
    float camDistance = 2.5f, targetDistance = 2.5f;
    const float zoomSmoothness = 0.18f;
    float camPitch = 0.0f, camYaw = 0.0f, camRoll = 0.0f;
    glm::vec3 camTarget = glm::vec3(0.0f);
    float deltaTime = 0.0f;
    glm::vec3 getCameraPos();
    void updateCamera();
    void mainLoop();

    // Helpers
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                      VkBuffer& buffer, VkDeviceMemory& memory);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    void createImage(uint32_t w, uint32_t h, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                     VkImage& image, VkDeviceMemory& memory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);
    void copyBufferToImageRegion(VkBuffer buffer, VkImage image, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    VkSampler createSampler();

    void initVulkan();  // orchestrates all init steps
    void cleanup();
};