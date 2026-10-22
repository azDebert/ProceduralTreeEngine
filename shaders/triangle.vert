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
layout(location = 10) in float inVertexFlex; // >0 = branch flexibility, 0 = ground, -1 = wind compass arrow

// Binding 1: Per-instance attributes (inputRate = INSTANCE)
layout(location = 3) in vec3 inInstancePos;
layout(location = 4) in vec3 inInstanceDir;
layout(location = 5) in vec3 inInstanceNormal;
layout(location = 6) in float inInstanceScale;
layout(location = 7) in vec3 inInstanceColor;
layout(location = 8) in float inInstancePhase;
layout(location = 9) in int inSpeciesType; // 0 = Deciduous, 1 = Sakura, 2 = Evergreen Pine
layout(location = 11) in float inInstanceFlex; // Accumulated branch flexibility at leaf attachment point

// Output colour and world-space normal sent to the fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;

// Computes cohesive biomechanical wind displacement using accumulated branch flexibility
vec3 computeWindDisplacement(vec3 worldPos, float flex, float phaseSeed) {
    if (flex <= 0.0001 || ubo.windStrength <= 0.001) {
        return vec3(0.0);
    }

    vec2 windDir = ubo.windDir;
    vec2 crossDir = vec2(-windDir.y, windDir.x);

    // Steady prevailing wind lean (increases smoothly with branch path flexibility)
    float steadyLean = flex * 0.24 * ubo.windStrength;

    // Travelling low-frequency bough gusts moving downwind across the scene
    float gustCoord = dot(worldPos.xy, windDir) * 0.85 - ubo.time * (1.8 + ubo.windStrength * 0.6);
    float primaryGust = (0.55 + 0.45 * sin(gustCoord + phaseSeed * 4.0)) * flex * 0.22 * ubo.windStrength;

    // Secondary high-frequency whip for thin outer twigs and hanging willow curtains
    float whipFlex = max(flex - 0.35, 0.0);
    float highFreqWhip = sin(ubo.time * 4.2 + gustCoord * 1.7 + worldPos.z * 2.1) * whipFlex * 0.14 * ubo.windStrength;

    // Subtle cross-wind eddy turbulence
    float crossEddy = cos(ubo.time * 2.7 + dot(worldPos.xy, crossDir) * 1.5 + phaseSeed * 6.0) * flex * 0.07 * ubo.windStrength;

    vec2 xyDisp = windDir * (steadyLean + primaryGust + highFreqWhip) + crossDir * crossEddy;
    // Slight vertical dip when branches bend strongly to preserve branch length
    float zDip = -length(xyDisp) * 0.18 * clamp(flex * 0.5, 0.0, 1.0);

    return vec3(xyDisp, zDip);
}

void main() {
    vec3 localPos = inPosition;
    vec3 localNormal = inNormal;
    vec3 finalColor = inColor;

    // Special case: 3D Wind Compass Arrow on the ground (inVertexFlex < -0.5)
    if (inInstanceScale <= 0.0 && inVertexFlex < -0.5) {
        vec2 w = ubo.windDir;
        // Rotate arrow around its local centre at (0.0, -3.1, -0.97) to point along ubo.windDir
        vec2 pivot = vec2(0.0, -3.1);
        vec2 rel = localPos.xy - pivot;
        // Scale arrow length slightly with windStrength
        rel.x *= (0.7 + 0.35 * ubo.windStrength);
        vec2 rotated = vec2(
            rel.x * w.x - rel.y * w.y,
            rel.x * w.y + rel.y * w.x
        );
        localPos.xy = pivot + rotated;
        gl_Position = ubo.proj * ubo.view * ubo.model * vec4(localPos, 1.0);
        fragColor = finalColor;
        fragNormal = mat3(ubo.model) * localNormal;
        return;
    }

    // Special case: Visible 3D Wind Streak Ribbons flying across the scene (inSpeciesType == 5)
    if (inInstanceScale > 0.0 && inSpeciesType == 5) {
        float streakScale = clamp(ubo.windStrength * 0.65, 0.0, 1.5);
        vec2 w = ubo.windDir;
        vec3 forward = vec3(w.x, w.y, 0.0);
        vec3 right = vec3(-w.y, w.x, 0.0);
        vec3 up = vec3(0.0, 0.0, 1.0);

        // Travel continuously along windDir and wrap smoothly inside [-6.0, 6.0]
        float travel = mod(inInstancePhase * 12.0 + ubo.time * (2.5 + ubo.windStrength * 2.2), 12.0) - 6.0;
        vec3 centerPos = inInstancePos + forward * travel;
        centerPos.z += sin(ubo.time * 3.0 + inInstancePhase * 15.0) * 0.15;

        vec3 scaled = vec3(inPosition.x * 0.06, inPosition.y * 1.35 * streakScale, inPosition.z * 0.06);
        localPos = centerPos + right * scaled.x + forward * scaled.y + up * scaled.z;
        localNormal = up;
        finalColor = vec3(0.82, 0.92, 0.98);

        gl_Position = ubo.proj * ubo.view * ubo.model * vec4(localPos, 1.0);
        fragColor = finalColor;
        fragNormal = mat3(ubo.model) * localNormal;
        return;
    }

    bool isOak       = (inSpeciesType == 0);
    bool isSakura    = (inSpeciesType == 1);
    bool isBirch     = (inSpeciesType == 2);
    bool isWillow    = (inSpeciesType == 3);
    bool isEvergreen = (inSpeciesType == 4);
    float phase      = fract(inInstancePhase);

    if (inInstanceScale > 0.0) {
        float s = mod(ubo.season, 4.0);

        // Cultivar-accurate Spring emergence colours (no pink on non-Sakura trees)
        vec3 springColor;
        if (isSakura) {
            // Delicate pale pink-white cherry blossom petals in early spring, bronze-copper emergence in late spring
            vec3 blossomPink = mix(vec3(0.98, 0.76, 0.84), vec3(1.0, 0.93, 0.96), phase);
            vec3 bronzeLeaf  = vec3(0.56, 0.32, 0.18);
            springColor = mix(blossomPink, bronzeLeaf, smoothstep(0.45, 0.88, s));
        } else if (isOak) {
            // English Oak emerges with warm bronze-olive young foliage
            springColor = mix(vec3(0.50, 0.44, 0.18), vec3(0.32, 0.60, 0.20), smoothstep(0.15, 0.85, s));
        } else if (isBirch) {
            // Silver Birch emerges crisp bright chartreuse green
            springColor = mix(vec3(0.45, 0.78, 0.24), inInstanceColor, smoothstep(0.20, 0.90, s));
        } else {
            // Weeping Willow emerges fresh yellow-lime green
            springColor = mix(vec3(0.52, 0.82, 0.25), inInstanceColor, smoothstep(0.20, 0.90, s));
        }

        // Summer vibrant green
        vec3 summerColor = inInstanceColor;

        // Species-accurate Autumn palette (yellow -> orange -> crimson)
        vec3 autumnYellow  = vec3(0.96, 0.78, 0.12);
        vec3 autumnOrange  = vec3(0.94, 0.42, 0.07);
        vec3 autumnCrimson = vec3(0.78, 0.12, 0.10);
        vec3 autumnBronze  = vec3(0.68, 0.38, 0.12);

        vec3 autumnColor;
        if (isSakura) {
            autumnColor = mix(autumnYellow, mix(autumnOrange, autumnCrimson, smoothstep(0.35, 0.9, phase)), smoothstep(0.1, 0.8, phase));
        } else if (isBirch) {
            // Birch turns brilliant golden-yellow with warm amber tips
            autumnColor = mix(autumnYellow, autumnOrange, smoothstep(0.5, 1.0, phase) * 0.65);
        } else if (isWillow) {
            autumnColor = mix(vec3(0.82, 0.76, 0.18), autumnYellow, phase);
        } else {
            // English Oak turns golden-orange, russet, and warm bronze
            autumnColor = mix(autumnYellow, mix(autumnOrange, autumnBronze, smoothstep(0.4, 1.0, phase)), smoothstep(0.1, 0.85, phase));
        }

        vec3 winterColor = isEvergreen ? inInstanceColor : autumnColor * 0.62;

        float seasonScale = 1.0;
        float fallOffsetZ = 0.0;
        vec2 fallDriftXY = vec2(0.0);
        float flutterAngle = 0.0;
        float attachWindWeight = 1.0;

        if (isEvergreen) {
            // Scots Pine keeps its 3D needle clusters all year, with subtle winter frost tint
            float winterFrost = (s >= 3.0) ? smoothstep(0.0, 0.4, s - 3.0) * (1.0 - smoothstep(0.6, 1.0, s - 3.0)) * 0.42 : 0.0;
            finalColor = mix(inInstanceColor, vec3(0.84, 0.90, 0.95), winterFrost);
            seasonScale = 1.0;
        }
        else if (s < 1.0) {
            // Spring (0.0 -> 1.0)
            float t = smoothstep(0.0, 0.80, clamp((s - phase * 0.22) / 0.78, 0.0, 1.0));
            finalColor = mix(springColor, summerColor, smoothstep(0.65, 1.0, s));
            seasonScale = isSakura ? mix(0.85, 1.0, t) : mix(0.35, 1.0, t);
        }
        else if (s < 2.0) {
            // Summer (1.0) -> Early/Mid Autumn (2.0):
            // Bough-clustered phase means low-phase branches turn yellow/orange while high-phase branches stay green!
            float u = s - 1.0;
            float branchStart = 0.15 + phase * 0.65;
            float colorT = smoothstep(branchStart, branchStart + 0.25, u);
            finalColor = mix(summerColor, autumnColor, colorT);
            seasonScale = 1.0;
        }
        else if (s < 3.0) {
            // Peak Autumn (2.0) -> Winter Leaf Fall (3.0):
            // Remaining green boughs finish turning colour, then all deciduous trees (including Oak in groves) shed leaves
            float u = s - 2.0;
            float finishTurn = smoothstep(0.0, 0.25, u);
            finalColor = mix(mix(summerColor, autumnColor, smoothstep(0.0, 0.4, 1.0 - phase + u)), winterColor, u * 0.75);

            float detachStart = 0.08 + phase * 0.58;
            if (u > detachStart) {
                float fallProgress = clamp((u - detachStart) / 0.34, 0.0, 1.0);
                float maxDrop = max(inInstancePos.z - (-0.98), 0.0);
                fallOffsetZ = -min(fallProgress * 3.8, maxDrop);

                float isInAir = step(-maxDrop + 0.01, fallOffsetZ);
                attachWindWeight = isInAir; // Stop swaying with upper branches once resting on the ground

                float swayPhase = ubo.time * 3.2 + phase * 25.0;
                vec2 windCarry = ubo.windDir * (fallProgress * 0.85 * ubo.windStrength);
                vec2 turbulence = vec2(sin(swayPhase), cos(swayPhase * 0.8)) * 0.18 * fallProgress * isInAir;
                fallDriftXY = windCarry + turbulence;
                flutterAngle = sin(swayPhase) * 0.75 * isInAir;

                seasonScale = 1.0 - smoothstep(0.78, 1.0, fallProgress);
            }
        }
        else {
            // Winter (3.0 -> 4.0): bare deciduous branches until spring buds emerge at s > 3.65
            float u = s - 3.0;
            finalColor = springColor;
            float sproutStart = 0.65 + phase * 0.22;
            seasonScale = smoothstep(sproutStart, 1.0, u) * 0.35;
        }

        vec3 forward = normalize(inInstanceDir);
        vec3 up = normalize(inInstanceNormal);
        vec3 right = normalize(cross(forward, up));
        up = cross(right, forward);

        // High-frequency leaf blade flutter on top of branch sway
        float flutterFreq = isWillow ? 3.2 : 5.5;
        float leafFlutter = sin(ubo.time * flutterFreq + dot(inInstancePos.xy, ubo.windDir) * 3.5 + phase * 12.0)
                          * 0.22 * ubo.windStrength * attachWindWeight + flutterAngle;
        float c = cos(leafFlutter);
        float sRot = sin(leafFlutter);
        vec3 tiltedRight = right * c + up * sRot;
        vec3 tiltedUp = -right * sRot + up * c;

        // Apply biomechanical wind displacement at the twig attachment point
        vec3 branchWind = computeWindDisplacement(inInstancePos, inInstanceFlex, phase) * attachWindWeight;

        vec3 scaled = inPosition * (inInstanceScale * seasonScale);
        localPos = inInstancePos
                 + branchWind
                 + vec3(fallDriftXY, fallOffsetZ)
                 + tiltedRight * scaled.x
                 + forward * scaled.y
                 + tiltedUp * scaled.z;
        localNormal = normalize(tiltedUp + inNormal.x * tiltedRight * 0.35);
    }
    else {
        // Trunk and branch cylinder vertices: bend using accumulated per-vertex branch flexibility
        localPos += computeWindDisplacement(localPos, inVertexFlex, 0.0);
    }

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(localPos, 1.0);
    fragColor = finalColor;
    fragNormal = mat3(ubo.model) * localNormal;
}