#version 450

// Input colour received from the vertex shader (smoothly blended across the triangle)
layout(location = 0) in vec3 fragColor;

// Final RGBA colour written to swapchain attachment 0
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}