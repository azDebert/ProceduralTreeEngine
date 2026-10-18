#version 450

// Input vertex attributes read from the bound Vulkan vertex buffer
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

// Output colour sent to the fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}