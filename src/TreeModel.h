#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// Iconic botanical tree species supported by the procedural generator
enum class TreeSpecies : int {
    EnglishOak = 0,
    SakuraBlossom = 1,
    SilverBirch = 2,
    WeepingWillow = 3,
    ScotsPine = 4,
    Count = 5
};

// Configuration parameters controlling the procedural tree shape
struct TreeParameters {
    uint32_t seed = 42;
    TreeSpecies species = TreeSpecies::EnglishOak;
    std::string speciesName = "English Oak";
    float trunkHeight = 2.0f;
    float trunkRadius = 0.25f;
    int recursionDepth = 5;       // How many levels of child branches
    float branchAngle = 35.0f;    // Angle at which branches split off
    float lengthShrink = 0.7f;    // How much shorter child branches are
    float radiusShrink = 0.6f;    // How much thinner child branches are
    float tropism = 0.08f;        // Vertical bend per level (+ upward sun-seeking, - weeping droop)
    glm::vec3 barkColor = { 0.42f, 0.27f, 0.14f };
    int radialSegments = 12;      // Smoothness of cylinders
    int leavesPerTwig = 14;       // Number of leaves spawned on each outer branch
    float leafScale = 0.18f;      // Base size of each individual leaf
};

// Returns real-world inspired botanical parameters for each tree species
inline TreeParameters getSpeciesPreset(TreeSpecies species, uint32_t seed = 42) {
    TreeParameters p{};
    p.seed = seed;
    p.species = species;

    switch (species) {
    case TreeSpecies::EnglishOak:
        p.speciesName = "English Oak (Quercus robur)";
        p.trunkHeight = 1.9f;
        p.trunkRadius = 0.28f;
        p.recursionDepth = 5;
        p.branchAngle = 42.0f;
        p.lengthShrink = 0.72f;
        p.radiusShrink = 0.62f;
        p.tropism = 0.06f;
        p.barkColor = { 0.38f, 0.25f, 0.14f };
        p.leavesPerTwig = 16;
        p.leafScale = 0.19f;
        break;

    case TreeSpecies::SakuraBlossom:
        p.speciesName = "Japanese Sakura (Prunus serrulata)";
        p.trunkHeight = 1.65f;
        p.trunkRadius = 0.24f;
        p.recursionDepth = 5;
        p.branchAngle = 48.0f;
        p.lengthShrink = 0.74f;
        p.radiusShrink = 0.58f;
        p.tropism = 0.04f;
        p.barkColor = { 0.28f, 0.16f, 0.16f }; // Dark reddish-mahogany cherry bark
        p.leavesPerTwig = 22;
        p.leafScale = 0.17f;
        break;

    case TreeSpecies::SilverBirch:
        p.speciesName = "Silver Birch (Betula pendula)";
        p.trunkHeight = 2.4f;
        p.trunkRadius = 0.16f;
        p.recursionDepth = 5;
        p.branchAngle = 28.0f;
        p.lengthShrink = 0.70f;
        p.radiusShrink = 0.56f;
        p.tropism = 0.10f;
        p.barkColor = { 0.85f, 0.83f, 0.79f }; // Distinctive chalky silver-white bark
        p.leavesPerTwig = 14;
        p.leafScale = 0.15f;
        break;

    case TreeSpecies::WeepingWillow:
        p.speciesName = "Weeping Willow (Salix babylonica)";
        p.trunkHeight = 2.1f;
        p.trunkRadius = 0.26f;
        p.recursionDepth = 6;                  // Extra recursion level for long hanging curtains
        p.branchAngle = 45.0f;
        p.lengthShrink = 0.76f;
        p.radiusShrink = 0.55f;
        p.tropism = -0.42f;                    // Strong negative tropism makes branches arch & hang down
        p.barkColor = { 0.36f, 0.30f, 0.20f };
        p.leavesPerTwig = 18;
        p.leafScale = 0.21f;
        break;

    case TreeSpecies::ScotsPine:
    default:
        p.speciesName = "Scots Pine (Pinus sylvestris)";
        p.trunkHeight = 2.5f;
        p.trunkRadius = 0.22f;
        p.recursionDepth = 5;
        p.branchAngle = 58.0f;
        p.lengthShrink = 0.65f;
        p.radiusShrink = 0.55f;
        p.tropism = -0.10f;
        p.barkColor = { 0.48f, 0.24f, 0.12f }; // Warm orange-reddish pine bark
        p.leavesPerTwig = 20;
        p.leafScale = 0.16f;
        break;
    }
    return p;
}

// Represents a single branch segment in 3D space
struct BranchSegment {
    glm::vec3 start;
    glm::vec3 end;
    float startRadius;
    float endRadius;
    glm::vec3 barkColor = { 0.42f, 0.27f, 0.14f };
};

// Renderer-independent per-leaf instance data
struct LeafInstance {
    glm::vec3 position;   // World-space attachment point on the twig
    glm::vec3 direction;  // Forward growth direction of the leaf
    glm::vec3 normal;     // Upward-facing surface normal of the leaf blade
    float scale;          // Uniform size multiplier for this leaf
    glm::vec3 color;      // Base summer green colour with per-leaf variation
    float phase;          // Stable per-leaf identity seed for seasonal timing
    int speciesType;      // 0 = Deciduous, 1 = Sakura, 2 = Evergreen Pine
};

// The standalone procedural tree data model
class TreeModel {
public:
    explicit TreeModel(const TreeParameters& params) : m_params(params) {
        generate();
    }

    const std::vector<BranchSegment>& getSegments() const {
        return m_segments;
    }

    const std::vector<LeafInstance>& getLeaves() const {
        return m_leaves;
    }

private:
    TreeParameters m_params;
    std::vector<BranchSegment> m_segments;
    std::vector<LeafInstance> m_leaves;

    void generate() {
        m_segments.clear();
        m_leaves.clear();
        std::mt19937 rng(m_params.seed);

        // Start trunk at origin pointing straight up along Z-axis
        glm::vec3 rootStart(0.0f, 0.0f, -1.0f);
        glm::vec3 rootEnd(0.0f, 0.0f, -1.0f + m_params.trunkHeight);

        std::uniform_real_distribution<float> treePhaseDist(0.0f, 1.0f);
        float initialPhase = treePhaseDist(rng);

        generateBranch(
            rootStart,
            rootEnd,
            m_params.trunkRadius,
            m_params.recursionDepth,
            glm::vec3(0.0f, 0.0f, 1.0f),
            initialPhase,
            rng
        );
    }

    void generateBranch(
        const glm::vec3& start,
        const glm::vec3& end,
        float radius,
        int depth,
        const glm::vec3& direction,
        float branchPhase,
        std::mt19937& rng
    ) {
        if (depth <= 0) return;

        float endRadius = radius * m_params.radiusShrink;
        m_segments.push_back({ start, end, radius, endRadius, m_params.barkColor });

        // Always draw exactly ONE seed for this twig's leaves so changing leavesPerTwig never alters branch geometry
        uint32_t twigLeafSeed = rng();

        // Spawn individual leaves along outer twigs (depth 1 and 2)
        if (depth <= 2 && m_params.leavesPerTwig > 0) {
            std::mt19937 leafRng(twigLeafSeed);
            std::uniform_real_distribution<float> alongDist(0.15f, 1.0f);
            std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * glm::pi<float>());
            std::uniform_real_distribution<float> tiltDist(-0.4f, 0.6f);
            std::uniform_real_distribution<float> scaleDist(0.75f, 1.30f);
            std::uniform_real_distribution<float> colorShift(-0.07f, 0.07f);
            std::uniform_real_distribution<float> localPhaseJitter(-0.12f, 0.12f);
            std::uniform_real_distribution<float> phaseDist(0.0f, 1.0f); // <--- ADD THIS LINE

            glm::vec3 rawPerp = glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(rawPerp) < 0.001f) {
                rawPerp = glm::cross(direction, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            glm::vec3 twigPerp1 = glm::normalize(rawPerp);
            glm::vec3 twigPerp2 = glm::normalize(glm::cross(direction, twigPerp1));

            for (int i = 0; i < m_params.leavesPerTwig; ++i) {
                float t = alongDist(leafRng);
                float theta = angleDist(leafRng);
                glm::vec3 radialOut = std::cos(theta) * twigPerp1 + std::sin(theta) * twigPerp2;

                float currentRadius = glm::mix(radius, endRadius, t);
                glm::vec3 leafPos = glm::mix(start, end, t) + radialOut * (currentRadius + 0.02f);

                glm::vec3 leafDir = glm::normalize(radialOut + direction * 0.4f + glm::vec3(0.0f, 0.0f, tiltDist(leafRng)));
                if (m_params.species == TreeSpecies::WeepingWillow) {
                    leafDir = glm::normalize(leafDir + glm::vec3(0.0f, 0.0f, -0.85f));
                }
                glm::vec3 leafRight = glm::normalize(glm::cross(leafDir, glm::vec3(0.0f, 0.0f, 1.0f) + radialOut * 0.2f));
                glm::vec3 leafNormal = glm::normalize(glm::cross(leafRight, leafDir));

                float shift = colorShift(leafRng);
                glm::vec3 baseColor;
                if (m_params.species == TreeSpecies::SakuraBlossom) {
                    baseColor = glm::vec3(0.96f + shift * 0.4f, 0.68f + shift * 1.1f, 0.78f + shift * 0.8f);
                }
                else if (m_params.species == TreeSpecies::WeepingWillow) {
                    baseColor = glm::vec3(0.32f + shift, 0.72f + shift * 1.1f, 0.22f + shift * 0.5f);
                }
                else if (m_params.species == TreeSpecies::ScotsPine) {
                    baseColor = glm::vec3(0.10f + shift * 0.5f, 0.42f + shift * 0.8f, 0.16f + shift * 0.5f);
                }
                else {
                    baseColor = glm::vec3(0.20f + shift, 0.62f + shift * 1.2f, 0.18f + shift * 0.5f);
                }
                glm::vec3 leafColor = glm::clamp(baseColor, glm::vec3(0.05f), glm::vec3(0.98f));

                float leafPhase = std::clamp(branchPhase + localPhaseJitter(leafRng), 0.0f, 0.99f);

                int sType = 0;
                if (m_params.species == TreeSpecies::SakuraBlossom) {
                    sType = 1;
                }
                else if (m_params.species == TreeSpecies::ScotsPine) {
                    sType = 2;
                }

                m_leaves.push_back({
                    leafPos,
                    leafDir,
                    leafNormal,
                    m_params.leafScale * scaleDist(leafRng),
                    leafColor,
                    phaseDist(leafRng), // Now phaseDist is defined and available!
                    sType
                    });
            }
        }

        if (depth == 1) return;

        int childCount = (depth >= 4) ? 3 : 2;
        glm::vec3 splitPoint = end;

        std::uniform_real_distribution<float> rotDist(0.0f, 2.0f * glm::pi<float>());
        std::uniform_real_distribution<float> lengthDist(0.80f, 0.95f);
        std::uniform_real_distribution<float> pitchJitter(-6.0f, 6.0f);
        std::uniform_real_distribution<float> branchPhaseStep(-0.22f, 0.22f);

        for (int i = 0; i < childCount; ++i) {
            float baseAngle = (static_cast<float>(i) / static_cast<float>(childCount)) * 2.0f * glm::pi<float>();
            float angleRot = baseAngle + rotDist(rng) * 0.25f;
            float pitch = glm::radians(m_params.branchAngle + pitchJitter(rng));

            glm::vec3 rawPerp = glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(rawPerp) < 0.001f) {
                rawPerp = glm::cross(direction, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            glm::vec3 perp1 = glm::normalize(rawPerp);
            glm::vec3 perp2 = glm::normalize(glm::cross(direction, perp1));

            // Apply gravitropism (upward sun-seeking or downward weeping willow droop)
            glm::vec3 unbentDir =
                direction * std::cos(pitch) +
                (perp1 * std::cos(angleRot) + perp2 * std::sin(angleRot)) * std::sin(pitch);
            glm::vec3 childDir = glm::normalize(unbentDir + glm::vec3(0.0f, 0.0f, m_params.tropism));

            float childLength = glm::length(end - start) * m_params.lengthShrink * lengthDist(rng);
            glm::vec3 childEnd = splitPoint + childDir * childLength;

            // Each major bough shifts its seasonal phase slightly so some branches turn orange earlier than others
            float childPhase = std::clamp(branchPhase + branchPhaseStep(rng), 0.05f, 0.95f);

            generateBranch(splitPoint, childEnd, endRadius, depth - 1, childDir, childPhase, rng);
        }
    }
};