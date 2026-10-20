#version 450

// 3D transformation matrices and seasonal state updated every frame via descriptor set 0, binding 0
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    float season; // Continuous season: 0.0 = Spring, 1.0 = Summer, 2.0 = Autumn, 3.0 = Winter
    float time;   // Elapsed time in seconds for gentle wind and falling animation
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
layout(location = 8) in float inInstancePhase;

// Output colour and world space normal sent to the fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

void main() {
    vec3 localPos = inPosition;
    vec3 localNormal = inNormal;
    vec3 finalColor = inColor;

// If inInstanceScale > 0.0, transform the canonical leaf mesh using the instance attributes
    if (inInstanceScale > 0.0) {
        float s = mod(ubo.season, 4.0);

        // 1. Seasonal Palette per leaf (using stable inInstancePhase for unique autumn hues)
        vec3 springColor = mix(vec3(0.55, 0.88, 0.32), vec3(0.96, 0.72, 0.82), step(0.82, inInstancePhase));
        vec3 summerColor = inInstanceColor;
        // Autumn ranges from golden yellow (phase 0) through bright orange to deep crimson (phase 1)
        vec3 autumnColor = mix(
            vec3(0.95, 0.75, 0.12),
            mix(vec3(0.92, 0.38, 0.08), vec3(0.75, 0.12, 0.10), smoothstep(0.4, 1.0, inInstancePhase)),
            smoothstep(0.0, 0.7, inInstancePhase)
        );
        vec3 winterColor = autumnColor * 0.65; // Dry brown/russet before vanishing

        // 2. Continuous Seasonal Colour Interpolation & Foliage Visibility
        float seasonScale = 1.0;
        float fallOffsetZ = 0.0;
        vec2 fallDriftXY = vec2(0.0);
        float flutterAngle = 0.0;

        if (s < 1.0) {
            // Spring (0.0) -> Summer (1.0): leaves grow from buds to full size and deepen in green
            float t = smoothstep(0.0, 0.85, clamp((s - inInstancePhase * 0.25) / 0.75, 0.0, 1.0));
            finalColor = mix(springColor, summerColor, s);
            seasonScale = mix(0.25, 1.0, t);
        }
        else if (s < 2.0) {
            // Summer (1.0) -> Autumn (2.0): staggered colour change based on leaf phase
            float u = s - 1.0;
            float colorT = smoothstep(inInstancePhase * 0.45, inInstancePhase * 0.45 + 0.55, u);
            finalColor = mix(summerColor, autumnColor, colorT);
            seasonScale = 1.0;
        }
        else if (s < 3.0) {
            // Autumn (2.0) -> Winter (3.0): leaves detach at staggered times, drift to ground, and shrink
            float u = s - 2.0;
            finalColor = mix(autumnColor, winterColor, u);

            float detachStart = inInstancePhase * 0.65;
            if (u > detachStart) {
                float fallProgress = clamp((u - detachStart) / 0.35, 0.0, 1.0);
                // Fall from twig height down to ground plane (Z = -1.0)
                float maxDrop = max(inInstancePos.z - (-1.0), 0.0);
                fallOffsetZ = -min(fallProgress * 3.5, maxDrop);

                // Gentle swirling horizontal drift while falling
                float swayPhase = ubo.time * 3.0 + inInstancePhase * 25.0;
                float isInAir = step(-maxDrop + 0.01, fallOffsetZ);
                fallDriftXY = vec2(sin(swayPhase), cos(swayPhase * 0.8)) * 0.18 * fallProgress * isInAir;
                flutterAngle = sin(swayPhase) * 0.6 * isInAir;

                // Shrink away once resting on the ground approaching Winter
                seasonScale = 1.0 - smoothstep(0.75, 1.0, fallProgress);
            }
        }
        else {
            // Winter (3.0) -> Spring (4.0): bare branches until late winter when tiny spring buds sprout
            float u = s - 3.0;
            finalColor = springColor;
            float sproutStart = 0.60 + inInstancePhase * 0.25;
            seasonScale = smoothstep(sproutStart, 1.0, u) * 0.25;
        }

        vec3 forward = normalize(inInstanceDir);
        vec3 up = normalize(inInstanceNormal);
        vec3 right = normalize(cross(forward, up));
        up = cross(right, forward);

        // Apply subtle wind/flutter rotation around the forward axis
        float c = cos(flutterAngle);
        float sRot = sin(flutterAngle);
        vec3 tiltedRight = right * c + up * sRot;
        vec3 tiltedUp = -right * sRot + up * c;

        // Rotate, scale, and position the leaf blade in tree space
        vec3 scaled = inPosition * (inInstanceScale * seasonScale);
        localPos = inInstancePos
                 + vec3(fallDriftXY, fallOffsetZ)
                 + tiltedRight * scaled.x
                 + forward * scaled.y
                 + tiltedUp * scaled.z;
        localNormal = tiltedUp;
    }

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(localPos, 1.0);
    fragColor = finalColor;
    fragNormal = mat3(ubo.model) * localNormal;
}