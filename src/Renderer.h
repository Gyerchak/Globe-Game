#pragma once
#include "Settings.h"
#include "Mesh.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <string>                // <-- add this

constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;   // <-- add this

struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos;
    float displacementScale;
};

struct PushConstants {
    int gridOverlay;
    int gridLineOverlay;
};

class Window;
class Camera;

class Renderer {
public:
    Renderer(Window* window, const AppSettings& settings);
    ~Renderer();
    void drawFrame(const Camera& camera, bool showGrid, bool showGridLines, float displacementScale);
    void waitIdle();

    SphereMesh sphereMesh;
    VkImage gridImage = VK_NULL_HANDLE, heightmapImage = VK_NULL_HANDLE, textureImage = VK_NULL_HANDLE;
    VkDeviceMemory gridMemory, heightmapMemory, textureMemory;
    VkImageView gridView, heightmapView, textureView;
    VkSampler gridSampler, heightmapSampler, textureSampler;

    void initResources();
    void loadTextureFromFile(const std::string& path);
    void loadHeightmapFromFile(const std::string& path);
    void loadGridFromFile(const std::string& path);

private:
    Window* window;
    AppSettings settings;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue, presentQueue;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores, renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    VkExtent2D offscreenExtent;
    VkImage offscreenColorImage, offscreenDepthImage;
    VkDeviceMemory offscreenColorMemory, offscreenDepthMemory;
    VkImageView offscreenColorView, offscreenDepthView;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void recreateSwapChain();
    void createCommandPool();
    void createSyncObjects();
    void createOffscreenResources();
    void createDescriptorSetLayout();
    void createPipeline();
    void createUniformBuffers();
    void createDescriptorSets();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, const Camera& camera, bool showGrid, bool showGridLines, float displacementScale);
    void updateUniforms(const Camera& camera, float displacementScale);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    void createImage(uint32_t w, uint32_t h, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags props, VkImage& image, VkDeviceMemory& memory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void copyBufferToImageRegion(VkBuffer buffer, VkImage image, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    VkSampler createSampler();
};