#version 450

// Input colour and normal received from the vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;

// Final RGBA colour written to swapchain attachment 0
layout(location = 0) out vec4 outColor;

void main() {
    // Directional sunlight coming from the top right front of the scene
    vec3 lightDir = normalize(vec3(0.5, -0.7, 0.8));
    vec3 normal = normalize(fragNormal);

    // Ambient sky light + Lambertian diffuse sunlight
    float ambient = 0.30;
    float diffuse = max(dot(normal, lightDir), 0.0) * 0.75;

    vec3 litColor = fragColor * (ambient + diffuse);
    outColor = vec4(litColor, 1.0);
}