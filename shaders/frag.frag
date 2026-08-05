#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(binding = 0) uniform sampler2D globeSampler;      // main globe
layout(binding = 2) uniform sampler2D gridSampler;       // grid overlay (R8, 32768x32768)

layout(push_constant) uniform PushConstants {
    int gridOverlay;      // 1 = show red fill
    int gridLineOverlay;  // 1 = show vector grid lines
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
        const float GRID_STEP  = 1.0;   // draw a line every 1 cell

        vec2 gridUV = uv * vec2(gridWidth, gridHeight);

        float distU = abs(fract(gridUV.x / GRID_STEP) - 0.5) * 2.0;
        float distV = abs(fract(gridUV.y / GRID_STEP) - 0.5) * 2.0;

        float dU = fwidth(gridUV.x);
        float dV = fwidth(gridUV.y);

        float lineWidthU = dU * 2.0 / GRID_STEP;
        float lineWidthV = dV * 2.0 / GRID_STEP;

        float lineU = 1.0 - smoothstep(0.0, lineWidthU, distU);
        float lineV = 1.0 - smoothstep(0.0, lineWidthV, distV);

        float gridLine = max(lineU, lineV);

        finalColor = mix(finalColor, vec4(1.0), gridLine * 0.8);
    }

    // Force alpha to 1.0 (harmless for B5G6R5, but safe)
    outColor = vec4(finalColor.rgb, 1.0);
}