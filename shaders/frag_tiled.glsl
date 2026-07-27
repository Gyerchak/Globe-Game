#version 450
layout(location = 0) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // Horizontal flip
    vec2 uv = vec2(1.0 - fragTexCoord.x, fragTexCoord.y);
    outColor = texture(texSampler, uv);
}
