#version 450

// 3D transformation matrices, seasonal state, and directional wind updated every frame
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    float season;       // Continuous season: 0.0 = Spring, 1.0 = Summer, 2.0 = Autumn, 3.0 = Winter
    float time;         // Elapsed time in seconds
    float windStrength; // Wind intensity multiplier
    vec2 windDir;       // Normalised 2D horizontal wind direction vector
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
layout(location = 9) in int inSpeciesType; // 0 = Deciduous, 1 = Sakura, 2 = Evergreen Pine

// Output colour and world-space normal sent to the fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

void main() {
    vec3 localPos = inPosition;
    vec3 localNormal = inNormal;
    vec3 finalColor = inColor;

    bool isSakura = (inSpeciesType == 1);
    bool isEvergreen = (inSpeciesType == 2);
    float phase = inInstancePhase;

    if (inInstanceScale > 0.0) {
        float s = mod(ubo.season, 4.0);

        // Botanical colour profiles
        vec3 springColor = isSakura ? vec3(0.98, 0.75, 0.82) : mix(vec3(0.35, 0.65, 0.22), vec3(0.55, 0.88, 0.32), phase);
        vec3 summerColor = isSakura ? vec3(0.18, 0.52, 0.20) : inInstanceColor; // Sakura turns vibrant dark green in summer!
        vec3 autumnColor = isSakura 
            ? vec3(0.92, 0.55, 0.60) 
            : mix(vec3(0.96, 0.78, 0.10), mix(vec3(0.94, 0.40, 0.06), vec3(0.78, 0.10, 0.10), smoothstep(0.4, 1.0, phase)), smoothstep(0.0, 0.7, phase));
        vec3 winterColor = isEvergreen ? inInstanceColor : autumnColor * 0.65;

        float seasonScale = 1.0;
        float fallOffsetZ = 0.0;
        vec2 fallDriftXY = vec2(0.0);
        float flutterAngle = 0.0;

        if (isEvergreen) {
            // Scots Pine: evergreen needles stay green year-round, unaffected by autumn/winter shedding
            finalColor = (s >= 3.0) ? mix(inInstanceColor, vec3(0.85, 0.90, 0.95), 0.35) : inInstanceColor;
            seasonScale = 1.0;
        }
        else if (s < 1.0) {
            // Spring: Sakura blooms pink; deciduous trees emerge with fresh green foliage
            float t = smoothstep(0.0, 0.85, clamp((s - phase * 0.25) / 0.75, 0.0, 1.0));
            finalColor = isSakura ? mix(vec3(0.98, 0.72, 0.80), summerColor, s) : mix(springColor, summerColor, smoothstep(0.4, 1.0, s));
            seasonScale = isSakura ? mix(0.85, 1.0, s) : mix(0.30, 1.0, t);
        }
        else if (s < 2.0) {
            // Summer: Vibrant dark green canopy across all deciduous species
            float u = s - 1.0;
            finalColor = mix(isSakura ? summerColor : springColor, summerColor, u);
            seasonScale = 1.0;
        }
        else if (s < 3.0) {
            // Autumn: Deciduous trees turn gold/crimson and shed leaves; Pine and non-shedding types are excluded
            float u = s - 2.0;
            float colorT = smoothstep(phase * 0.55, phase * 0.55 + 0.40, u);
            finalColor = mix(summerColor, autumnColor, colorT);

            float detachStart = phase * 0.65;
            if (u > detachStart) {
                float fallProgress = clamp((u - detachStart) / 0.35, 0.0, 1.0);
                float maxDrop = max(inInstancePos.z - (-0.98), 0.0);
                fallOffsetZ = -min(fallProgress * 3.5, maxDrop);

                float swayPhase = ubo.time * 3.0 + phase * 25.0;
                float isInAir = step(-maxDrop + 0.01, fallOffsetZ);
                vec2 windCarry = ubo.windDir * (fallProgress * 0.75 * ubo.windStrength);
                vec2 turbulence = vec2(sin(swayPhase), cos(swayPhase * 0.8)) * 0.16 * fallProgress * isInAir;
                fallDriftXY = windCarry + turbulence;
                flutterAngle = sin(swayPhase) * 0.7 * isInAir;

                seasonScale = 1.0 - smoothstep(0.75, 1.0, fallProgress);
            }
        }
        else {
            // Winter: Bare deciduous branches; buds emerge late winter
            float u = s - 3.0;
            finalColor = springColor;
            float sproutStart = 0.60 + phase * 0.25;
            seasonScale = smoothstep(sproutStart, 1.0, u) * 0.30;
        }

        vec3 forward = normalize(inInstanceDir);
        vec3 up = normalize(inInstanceNormal);
        vec3 right = normalize(cross(forward, up));
        up = cross(right, forward);

        float leafFlutter = sin(ubo.time * 4.5 + dot(inInstancePos.xy, ubo.windDir) * 3.0 + phase * 10.0)
                          * 0.18 * ubo.windStrength + flutterAngle;
        float c = cos(leafFlutter);
        float sRot = sin(leafFlutter);
        vec3 tiltedRight = right * c + up * sRot;
        vec3 tiltedUp = -right * sRot + up * c;

        vec3 scaled = inPosition * (inInstanceScale * seasonScale);
        localPos = inInstancePos
                 + vec3(fallDriftXY, fallOffsetZ)
                 + tiltedRight * scaled.x
                 + forward * scaled.y
                 + tiltedUp * scaled.z;
        localNormal = tiltedUp;
    }

    // Realistic Wind Biomechanics:
    // Full-tree physical lean proportional to height and wind strength (ground fixed at Z = -1.0)
    // Cantilever branch whipping and gust turbulence
    float heightAboveGround = max(localPos.z - (-1.0), 0.0);
    if (heightAboveGround > 0.01 && ubo.windStrength > 0.0) {
        float heightFactor = heightAboveGround / 3.0; // Normalized tree height
        
        // Quadratic cantilever bending profile (base stays rigid, upper canopy bends smoothly)
        float bendDisplacement = (heightFactor * heightFactor * 0.18 + heightFactor * 0.08) * ubo.windStrength;
        vec2 baseLean = ubo.windDir * bendDisplacement;

        // Dynamic gust turbulence across the canopy
        float gustWave = sin(ubo.time * 3.2 - dot(localPos.xy, ubo.windDir) * 0.8);
        float branchWhip = heightFactor * heightFactor * (0.06 * gustWave) * ubo.windStrength;
        vec2 dynamicWhip = ubo.windDir * branchWhip;

        localPos.xy += baseLean + dynamicWhip;
    }

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(localPos, 1.0);
    fragColor = finalColor;
    fragNormal = mat3(ubo.model) * localNormal;
}