#version 450

// 3D transformation matrices updated every frame via descriptor set 0, binding 0
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

// Binding 0: Per-vertex attributes (inputRate = VERTEX)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

// Binding 1: Per-instance attributes (inputRate = INSTANCE)
layout(location = 3) in vec3 inInstancePos;
layout(location = 4) in vec3 inInstanceDir;
layout(location = 5) in vec3 inInstanceNormal;
layout(location = 6) in float inInstanceScale;
layout(location = 7) in vec3 inInstanceColor;

// Output colour and world space normal sent to the fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

void main() {
    vec3 localPos = inPosition;
    vec3 localNormal = inNormal;
    vec3 finalColor = inColor;

    // If inInstanceScale > 0.0, transform the canonical leaf mesh using the instance attributes
    if (inInstanceScale > 0.0) {
        vec3 forward = normalize(inInstanceDir);
        vec3 up = normalize(inInstanceNormal);
        vec3 right = normalize(cross(forward, up));
        up = cross(right, forward);

        // Rotate and scale the canonical leaf vertex into tree-space
        vec3 scaled = inPosition * inInstanceScale;
        localPos = inInstancePos + right * scaled.x + forward * scaled.y + up * scaled.z;
        localNormal = up;
        finalColor = inInstanceColor;
    }

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(localPos, 1.0);
    fragColor = finalColor;
    fragNormal = mat3(ubo.model) * localNormal;
}