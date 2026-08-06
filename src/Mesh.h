#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
};

class SphereMesh {
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    void generate(uint32_t lonSegs = 512, uint32_t latSegs = 256);
    void upload(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkPhysicalDevice physDev);
    void cleanup(VkDevice device);
};