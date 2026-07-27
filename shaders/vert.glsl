#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inTexCoord;

layout(binding = 1) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout(location = 0) out vec2 fragTexCoord;

void main() {
    fragTexCoord = inTexCoord;
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
}
