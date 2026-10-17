#version 450

// Output colour sent to the fragment shader
layout(location = 0) out vec3 fragColor;

// Hardcoded triangle positions in Normalised Device Coordinates (X, Y)
// (0, 0) is the centre of the screen, (-1, -1) is top-left, (1, 1) is bottom-right
vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5), // Top corner
    vec2( 0.5,  0.5), // Bottom-right corner
    vec2(-0.5,  0.5)  // Bottom-left corner
);

// RGB colours for each corner
vec3 colors[3] = vec3[](
    vec3(1.0, 0.2, 0.2),
    vec3(0.2, 0.8, 0.3),
    vec3(0.2, 0.4, 1.0)
);

void main() {
    // gl_VertexIndex is 0, 1, or 2 depending on which corner the GPU is processing
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}