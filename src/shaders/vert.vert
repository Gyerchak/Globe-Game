#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inTexCoord;

layout(binding = 1) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    float displacementScale;
} ubo;

layout(binding = 3) uniform sampler2D heightmapSampler;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    // Use the same UV orientation for heightmap samples as the globe texture.
    vec2 sampleUV = vec2(1.0 - inTexCoord.x, inTexCoord.y);
    float height = texture(heightmapSampler, sampleUV).r - 0.5;

    // Displace along the unit-sphere normal (the vertex position direction for a unit sphere)
    vec3 displacedPos = inPos + normalize(inPos) * height * ubo.displacementScale;

    fragTexCoord = inTexCoord;
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(displacedPos, 1.0);
}