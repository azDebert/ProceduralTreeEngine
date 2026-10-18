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
layout(location = 2) in vec3 inNormal;

// Output colour and world space normal sent to the fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;

    // Rotate the surface normal into world space so lighting reacts as the tree rotates
    fragNormal = mat3(ubo.model) * inNormal;
}