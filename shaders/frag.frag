#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D globeSampler;      // main globe
layout(binding = 2) uniform sampler2D gridSampler;       // grid overlay (R8, 32768x32768)

layout(push_constant) uniform PushConstants {
    int gridOverlay;      // 1 = show red fill
    int gridLineOverlay;  // 1 = show vector lines
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = vec2(1.0 - fragTexCoord.x, fragTexCoord.y);
    vec4 globeColor = texture(globeSampler, uv);

    // --- Grid fill (red where grid value > 0) ---
    vec4 filledColor = globeColor;
    if (pc.gridOverlay != 0) {
        float g = texture(gridSampler, uv).r;
        vec4 gridColor = vec4(1.0, 0.0, 0.0, g);
        filledColor = mix(globeColor, gridColor, g);
    }

    // --- Grid lines (cell boundaries) ---
    vec4 finalColor = filledColor;
    if (pc.gridLineOverlay != 0) {
        const float gridWidth  = 32768.0;
        const float gridHeight = 32768.0;

        // Compute grid cell coordinates
        vec2 gridUV = uv * vec2(gridWidth, gridHeight);

        // Screen-space derivatives give the size of one pixel in grid-cell units
        vec2 dpdx = fwidth(gridUV);

        // Distance from cell edge in U and V directions (0 at edge, 0.5 at center)
        vec2 dist = abs(fract(gridUV) - 0.5) * 2.0;   // 0 at edge, 1 at center

        // Line width: one screen pixel wide in each direction
        vec2 lineWidth = dpdx * 1.0;   // adjust multiplier for thicker lines

        // Create soft lines: 1.0 at cell edges, 0.0 inside cell
        float line = 1.0 - smoothstep(0.0, lineWidth, dist);

        // Take the maximum of U and V lines (draws both horizontal and vertical lines)
        float gridLine = max(line.x, line.y);

        // Blend a white line over the final color
        finalColor = mix(finalColor, vec4(1.0), gridLine * 0.8);   // 0.8 opacity for visibility
    }

    outColor = finalColor;
}