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
    // Sample the heightmap at the texture coordinate (assumed equirectangular)
    float height = texture(heightmapSampler, inTexCoord).r;

    // Displace along the unit-sphere normal (the vertex position direction for a unit sphere)
    vec3 displacedPos = inPos + normalize(inPos) * height * ubo.displacementScale;

    fragTexCoord = inTexCoord;
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(displacedPos, 1.0);
}