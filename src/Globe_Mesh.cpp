#include "GlobeApp.h"

Vertex::VkVertexInputBindingDescription Vertex::getBindingDescription() {
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

void GlobeApp::prepareSphere() {
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