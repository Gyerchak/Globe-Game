#include "Mesh.h"
#include <cstring>
#include <stdexcept>

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription b{};
    b.binding = 0;
    b.stride = sizeof(Vertex);
    b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return b;
}

std::array<VkVertexInputAttributeDescription, 2> Vertex::getAttributeDescriptions() {
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

void SphereMesh::generate(uint32_t lonSegs, uint32_t latSegs) {
    vertices.clear();
    indices.clear();
    for (uint32_t j = 0; j <= latSegs; ++j) {
        float theta = (float)j * glm::pi<float>() / (float)latSegs;
        float y = glm::cos(theta), sinTheta = glm::sin(theta);
        for (uint32_t i = 0; i <= lonSegs; ++i) {
            float phi = (float)i * 2.0f * glm::pi<float>() / (float)lonSegs;
            float x = glm::cos(phi) * sinTheta;
            float z = glm::sin(phi) * sinTheta;
            vertices.push_back({{x,y,z}, {(float)i / (float)lonSegs, (float)j / (float)latSegs}});
        }
    }
    for (uint32_t j = 0; j < latSegs; ++j) {
        for (uint32_t i = 0; i < lonSegs; ++i) {
            uint32_t first = j * (lonSegs + 1) + i;
            uint32_t second = first + lonSegs + 1;
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
}

static uint32_t findMemoryType(VkPhysicalDevice physDev, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("failed to find suitable memory type!");
}

static void createBuffer(VkDevice device, VkPhysicalDevice physDev, VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory) {
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
    ai.memoryTypeIndex = findMemoryType(physDev, req.memoryTypeBits, props);
    if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate buffer memory!");
    vkBindBufferMemory(device, buffer, memory, 0);
}

static void copyBuffer(VkDevice device, VkCommandPool cmdPool, VkQueue queue, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = cmdPool;
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
    vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, cmdPool, 1, &cmd);
}

void SphereMesh::upload(VkDevice device, VkCommandPool cmdPool, VkQueue queue, VkPhysicalDevice physDev) {
    VkDeviceSize vbSize = sizeof(Vertex) * vertices.size();
    VkBuffer staging; VkDeviceMemory stagingMem;
    createBuffer(device, physDev, vbSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
    void* data; vkMapMemory(device, stagingMem, 0, vbSize, 0, &data);
    memcpy(data, vertices.data(), vbSize); vkUnmapMemory(device, stagingMem);
    createBuffer(device, physDev, vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
    copyBuffer(device, cmdPool, queue, staging, vertexBuffer, vbSize);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    VkDeviceSize ibSize = sizeof(uint32_t) * indices.size();
    createBuffer(device, physDev, ibSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
    vkMapMemory(device, stagingMem, 0, ibSize, 0, &data);
    memcpy(data, indices.data(), ibSize); vkUnmapMemory(device, stagingMem);
    createBuffer(device, physDev, ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
    copyBuffer(device, cmdPool, queue, staging, indexBuffer, ibSize);
    vkDestroyBuffer(device, staging, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);
}

void SphereMesh::cleanup(VkDevice device) {
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);
}