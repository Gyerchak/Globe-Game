#version 450
layout(location = 0) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D texSamplers[4]; // up to 4 tiles (2x2)

layout(push_constant) uniform PushConstants {
    int waterOverlay;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    // Determine tile indices (assuming 2x2 grid)
    int tileX = int(floor(fragTexCoord.x * 2.0));
    int tileY = int(floor(fragTexCoord.y * 2.0));
    int tileIdx = tileY * 2 + tileX;

    // Local UV within the tile
    vec2 tileUV = vec2(fragTexCoord.x * 2.0 - float(tileX),
                       fragTexCoord.y * 2.0 - float(tileY));

    // Clamp to avoid edge artifacts
    tileUV = clamp(tileUV, 0.0, 1.0);

    outColor = texture(texSamplers[tileIdx], tileUV);
}
