#version 450

// 3D transformation matrices updated every frame via descriptor set 0, binding 0
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// Input vertex attributes read from the bound Vulkan vertex buffer
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Output colour sent to the fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}