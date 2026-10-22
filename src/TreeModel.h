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
    float tropism = 0.08f;        // Vertical bend per level (+ upward, - hanging droop)
    float windStiffness = 1.0f;   // Species wood stiffness (higher = stiffer trunk/boughs)
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
        p.trunkHeight = 1.85f;
        p.trunkRadius = 0.30f;
        p.recursionDepth = 5;
        p.branchAngle = 44.0f;
        p.lengthShrink = 0.73f;
        p.radiusShrink = 0.62f;
        p.tropism = 0.05f;
        p.windStiffness = 1.35f; // Heavy, sturdy hardwood boughs
        p.barkColor = { 0.36f, 0.24f, 0.14f };
        p.leavesPerTwig = 26;
        p.leafScale = 0.21f;
        break;

    case TreeSpecies::SakuraBlossom:
        p.speciesName = "Japanese Sakura (Prunus serrulata)";
        p.trunkHeight = 1.60f;
        p.trunkRadius = 0.25f;
        p.recursionDepth = 5;
        p.branchAngle = 50.0f;
        p.lengthShrink = 0.75f;
        p.radiusShrink = 0.58f;
        p.tropism = 0.03f;
        p.windStiffness = 1.05f;
        p.barkColor = { 0.26f, 0.15f, 0.15f }; // Dark reddish-mahogany bark
        p.leavesPerTwig = 32;
        p.leafScale = 0.19f;
        break;

    case TreeSpecies::SilverBirch:
        p.speciesName = "Silver Birch (Betula pendula)";
        p.trunkHeight = 2.35f;
        p.trunkRadius = 0.17f;
        p.recursionDepth = 5;
        p.branchAngle = 27.0f;
        p.lengthShrink = 0.72f;
        p.radiusShrink = 0.56f;
        p.tropism = 0.08f;
        p.windStiffness = 0.70f; // Slender, whippy stems
        p.barkColor = { 0.86f, 0.84f, 0.80f }; // Chalk-white bark
        p.leavesPerTwig = 24;
        p.leafScale = 0.16f;
        break;

    case TreeSpecies::WeepingWillow:
        p.speciesName = "Weeping Willow (Salix babylonica)";
        p.trunkHeight = 2.05f;
        p.trunkRadius = 0.28f;
        p.recursionDepth = 6;
        p.branchAngle = 42.0f;
        p.lengthShrink = 0.78f;
        p.radiusShrink = 0.56f;
        p.tropism = -0.58f;      // Strong downward curtain droop on outer branches
        p.windStiffness = 0.45f; // Long flexible hanging whips
        p.barkColor = { 0.35f, 0.29f, 0.19f };
        p.leavesPerTwig = 28;
        p.leafScale = 0.23f;
        break;

    case TreeSpecies::ScotsPine:
    default:
        p.speciesName = "Scots Pine (Pinus sylvestris)";
        p.trunkHeight = 2.45f;
        p.trunkRadius = 0.24f;
        p.recursionDepth = 5;
        p.branchAngle = 64.0f;
        p.lengthShrink = 0.66f;
        p.radiusShrink = 0.56f;
        p.tropism = -0.06f;
        p.windStiffness = 1.25f;
        p.barkColor = { 0.48f, 0.24f, 0.13f }; // Flaky orange-reddish upper pine bark
        p.leavesPerTwig = 30;
        p.leafScale = 0.20f;
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
    float startFlex = 0.0f; // Biomechanical wind flexibility at segment start
    float endFlex = 0.0f;   // Biomechanical wind flexibility at segment end
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
    float flexibility = 0.5f; // Accumulated branch flexibility so hanging tips swing properly
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

    static glm::vec3 safeNormalise(const glm::vec3& v, const glm::vec3& fallback = glm::vec3(0.0f, 0.0f, 1.0f)) {
        float len = glm::length(v);
        return (len > 0.0001f) ? (v / len) : fallback;
    }

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
            0.0f,
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
        float parentFlex,
        std::mt19937& rng
    ) {
        if (depth <= 0) return;

        float segLength = glm::length(end - start);
        if (segLength <= 0.0002f) return;

        float endRadius = std::max(radius * m_params.radiusShrink, 0.002f);
        if (endRadius > radius) {
            endRadius = radius;
        }

        // Accumulate biomechanical flexibility along the branch path from the trunk root.
        // Thinner outer or hanging branches gain much more flexibility than the thick trunk.
        float stiffness = std::max(m_params.windStiffness, 0.2f);
        float compliance = segLength / (std::pow(std::max(radius, 0.015f), 0.75f) * stiffness * 6.5f);
        float endFlex = parentFlex + compliance;

        m_segments.push_back({ start, end, radius, endRadius, m_params.barkColor, parentFlex, endFlex });

        // Draw one deterministic seed for this segment's leaves so changing leavesPerTwig never alters branch geometry
        uint32_t twigLeafSeed = rng();

        // Spawn foliage along outer twigs (depths 1, 2, and 3 for full crowns)
        int maxFoliageDepth = (m_params.species == TreeSpecies::WeepingWillow) ? 4 : 3;
        if (depth <= maxFoliageDepth && m_params.leavesPerTwig > 0) {
            std::mt19937 leafRng(twigLeafSeed);
            std::uniform_real_distribution<float> alongDist(0.08f, 1.0f);
            std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * glm::pi<float>());
            std::uniform_real_distribution<float> tiltDist(-0.35f, 0.45f);
            std::uniform_real_distribution<float> scaleDist(0.80f, 1.35f);
            std::uniform_real_distribution<float> colorShift(-0.05f, 0.05f);
            std::uniform_real_distribution<float> localPhaseJitter(-0.08f, 0.08f);
            std::uniform_real_distribution<float> clusterSpread(0.01f, 0.09f);

            glm::vec3 rawPerp = glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(rawPerp) < 0.001f) {
                rawPerp = glm::cross(direction, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            glm::vec3 twigPerp1 = safeNormalise(rawPerp);
            glm::vec3 twigPerp2 = safeNormalise(glm::cross(direction, twigPerp1));

            // Inner twigs (depth 3) carry fewer leaves than terminal tips (depth 1)
            int countForDepth = m_params.leavesPerTwig;
            if (depth == 3) countForDepth = std::max(m_params.leavesPerTwig / 2, 1);
            if (depth == 4) countForDepth = std::max(m_params.leavesPerTwig / 3, 1);

            int sType = static_cast<int>(m_params.species);

            for (int i = 0; i < countForDepth; ++i) {
                float t = alongDist(leafRng);
                // Golden-angle spiral phyllotaxis around the twig with slight natural jitter
                float goldenAngle = static_cast<float>(i) * 2.39996f + angleDist(leafRng) * 0.25f;
                glm::vec3 radialOut = std::cos(goldenAngle) * twigPerp1 + std::sin(goldenAngle) * twigPerp2;

                float currentRadius = glm::mix(radius, endRadius, t);
                float radialOffset = currentRadius + clusterSpread(leafRng);
                glm::vec3 leafPos = glm::mix(start, end, t) + radialOut * radialOffset;

                glm::vec3 leafDir = safeNormalise(radialOut + direction * 0.45f + glm::vec3(0.0f, 0.0f, tiltDist(leafRng)));
                if (m_params.species == TreeSpecies::WeepingWillow) {
                    // Willow blades hang downward along the curtain
                    leafDir = safeNormalise(radialOut * 0.25f + glm::vec3(0.0f, 0.0f, -1.0f));
                }
                else if (m_params.species == TreeSpecies::ScotsPine) {
                    // Pine needle fascicles angle slightly upward from the shoot
                    leafDir = safeNormalise(radialOut * 0.6f + direction * 0.5f + glm::vec3(0.0f, 0.0f, 0.35f));
                }

                glm::vec3 leafRight = safeNormalise(glm::cross(leafDir, glm::vec3(0.0f, 0.0f, 1.0f) + radialOut * 0.25f), glm::vec3(1.0f, 0.0f, 0.0f));
                glm::vec3 leafNormal = safeNormalise(glm::cross(leafRight, leafDir));

                float shift = colorShift(leafRng);
                glm::vec3 baseColor;
                switch (m_params.species) {
                case TreeSpecies::EnglishOak:
                    // Deep forest summer green
                    baseColor = glm::vec3(0.16f + shift * 0.6f, 0.48f + shift, 0.14f + shift * 0.4f);
                    break;
                case TreeSpecies::SakuraBlossom:
                    // Rich dark cherry-leaf summer green (spring pink & bronze handled in shader)
                    baseColor = glm::vec3(0.18f + shift * 0.6f, 0.50f + shift, 0.18f + shift * 0.5f);
                    break;
                case TreeSpecies::SilverBirch:
                    // Crisp medium emerald green
                    baseColor = glm::vec3(0.24f + shift * 0.7f, 0.60f + shift, 0.20f + shift * 0.5f);
                    break;
                case TreeSpecies::WeepingWillow:
                    // Fresh yellow-lime willow green
                    baseColor = glm::vec3(0.34f + shift * 0.8f, 0.68f + shift, 0.22f + shift * 0.5f);
                    break;
                case TreeSpecies::ScotsPine:
                default:
                    // Dark blue-green Scots Pine needles
                    baseColor = glm::vec3(0.09f + shift * 0.4f, 0.36f + shift * 0.7f, 0.16f + shift * 0.4f);
                    break;
                }
                glm::vec3 leafColor = glm::clamp(baseColor, glm::vec3(0.04f), glm::vec3(0.98f));

                // Keep leaf phase tightly clustered around its parent bough's phase
                // so whole branches change autumn colour together while other branches stay green
                float leafPhase = std::clamp(branchPhase + localPhaseJitter(leafRng), 0.01f, 0.99f);
                float leafFlex = glm::mix(parentFlex, endFlex, t) + 0.08f;

                m_leaves.push_back({
                    leafPos,
                    leafDir,
                    leafNormal,
                    m_params.leafScale * scaleDist(leafRng),
                    leafColor,
                    leafPhase,
                    sType,
                    leafFlex
                    });
            }
        }

        if (depth == 1) return;

        // Species-specific branching structure and canopy gaps
        int childCount = 2;
        if (m_params.species == TreeSpecies::ScotsPine) {
            childCount = (depth >= 3) ? 4 : 3; // Whorled tiers around the pine leader
        }
        else if (m_params.species == TreeSpecies::WeepingWillow) {
            childCount = (depth >= 5) ? 3 : 2; // Arching scaffold splits into long 2-way hanging curtains
        }
        else {
            childCount = (depth >= 3) ? 3 : 2;
        }

        glm::vec3 splitPoint = end;

        std::uniform_real_distribution<float> rotDist(0.0f, 2.0f * glm::pi<float>());
        std::uniform_real_distribution<float> lengthDist(0.82f, 0.98f);
        std::uniform_real_distribution<float> pitchJitter(-6.0f, 6.0f);
        std::uniform_real_distribution<float> boughPhaseSpread(-0.28f, 0.28f);

        float baseRotationOffset = rotDist(rng);

        for (int i = 0; i < childCount; ++i) {
            float baseAngle = baseRotationOffset + (static_cast<float>(i) / static_cast<float>(childCount)) * 2.0f * glm::pi<float>();
            float angleRot = baseAngle + rotDist(rng) * 0.16f;

            // Scots Pine keeps a straight central leader shoot on child 0 at upper depths
            bool isPineLeader = (m_params.species == TreeSpecies::ScotsPine && i == 0 && depth >= 4);
            float pitchDeg = isPineLeader ? 6.0f : (m_params.branchAngle + pitchJitter(rng));
            float pitch = glm::radians(pitchDeg);

            glm::vec3 rawPerp = glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(rawPerp) < 0.001f) {
                rawPerp = glm::cross(direction, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            glm::vec3 perp1 = safeNormalise(rawPerp);
            glm::vec3 perp2 = safeNormalise(glm::cross(direction, perp1));

            glm::vec3 unbentDir =
                direction * std::cos(pitch) +
                (perp1 * std::cos(angleRot) + perp2 * std::sin(angleRot)) * std::sin(pitch);

            // Weeping Willow arches upward on high scaffold limbs (depth >= 5) then droops strongly downward
            float effectiveTropism = m_params.tropism;
            if (m_params.species == TreeSpecies::WeepingWillow) {
                effectiveTropism = (depth >= 5) ? 0.18f : m_params.tropism;
            }

            glm::vec3 childDir = safeNormalise(unbentDir + glm::vec3(0.0f, 0.0f, effectiveTropism));
            float shrink = isPineLeader ? 0.82f : m_params.lengthShrink;
            float childLength = std::max(segLength * shrink * lengthDist(rng), 0.05f);
            glm::vec3 childEnd = splitPoint + childDir * childLength;

            // Major boughs get distinct seasonal phases so some branches turn yellow/orange while others stay green
            float phaseStep = (depth >= m_params.recursionDepth - 1) ? boughPhaseSpread(rng) : (boughPhaseSpread(rng) * 0.25f);
            float childPhase = std::clamp(branchPhase + phaseStep, 0.02f, 0.98f);

            generateBranch(splitPoint, childEnd, endRadius, depth - 1, childDir, childPhase, endFlex, rng);
        }

        // Spawn a lateral side-shoot from the middle of intermediate boughs to fill interior canopy gaps
        if (depth >= 3 && depth < m_params.recursionDepth && m_params.species != TreeSpecies::ScotsPine) {
            float lateralT = 0.55f;
            glm::vec3 lateralStart = glm::mix(start, end, lateralT);
            float lateralRadius = glm::mix(radius, endRadius, lateralT) * 0.72f;
            float lateralFlex = glm::mix(parentFlex, endFlex, lateralT);

            float latAngle = baseRotationOffset + 1.57f;
            glm::vec3 rawPerp = glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(rawPerp) < 0.001f) {
                rawPerp = glm::cross(direction, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            glm::vec3 perp1 = safeNormalise(rawPerp);
            glm::vec3 perp2 = safeNormalise(glm::cross(direction, perp1));

            glm::vec3 lateralDir = safeNormalise(
                direction * 0.5f +
                (perp1 * std::cos(latAngle) + perp2 * std::sin(latAngle)) * 0.85f +
                glm::vec3(0.0f, 0.0f, m_params.tropism * 0.8f)
            );
            float lateralLength = std::max(segLength * m_params.lengthShrink * 0.75f, 0.05f);
            glm::vec3 lateralEnd = lateralStart + lateralDir * lateralLength;

            generateBranch(lateralStart, lateralEnd, lateralRadius, depth - 2, lateralDir, branchPhase, lateralFlex, rng);
        }
    }
};