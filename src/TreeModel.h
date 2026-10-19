#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <random>

// Configuration parameters controlling the procedural tree shape
struct TreeParameters {
    uint32_t seed = 42;
    float trunkHeight = 2.0f;
    float trunkRadius = 0.25f;
    int recursionDepth = 5;       // How many levels of child branches
    float branchAngle = 35.0f;    // Angle at which branches split off
    float lengthShrink = 0.7f;    // How much shorter child branches are
    float radiusShrink = 0.6f;    // How much thinner child branches are
    int radialSegments = 12;      // Smoothness of cylinders
    int leavesPerTwig = 14;       // Number of leaves spawned on each outer branch
    float leafScale = 0.18f;      // Base size of each individual leaf};
};

// Represents a single branch segment in 3D space
struct BranchSegment {
    glm::vec3 start;
    glm::vec3 end;
    float startRadius;
    float endRadius;
};

// Renderer-independent per-leaf instance data
struct LeafInstance {
    glm::vec3 position;   // World-space attachment point on the twig
    glm::vec3 direction;  // Forward growth direction of the leaf
    glm::vec3 normal;     // Upward-facing surface normal of the leaf blade
    float scale;          // Uniform size multiplier for this leaf
    glm::vec3 color;      // Per-leaf colour variation
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
        std::uniform_real_distribution<float> angleJitter(-5.0f, 5.0f);

        // Start trunk at origin pointing straight up along Z-axis
        glm::vec3 rootStart(0.0f, 0.0f, -1.0f);
        glm::vec3 rootEnd(0.0f, 0.0f, -1.0f + m_params.trunkHeight);

        generateBranch(rootStart, rootEnd, m_params.trunkRadius, m_params.recursionDepth, glm::vec3(0.0f, 0.0f, 1.0f), rng);
    }

    void generateBranch(
        const glm::vec3& start,
        const glm::vec3& end,
        float radius,
        int depth,
        const glm::vec3& direction,
        std::mt19937& rng
    ) {
        if (depth <= 0) return;

        float endRadius = radius * m_params.radiusShrink;
        m_segments.push_back({ start, end, radius, endRadius });

        // Spawn individual leaves along outer twigs (depth 1 and 2)
        if (depth <= 2 && m_params.leavesPerTwig > 0) {
            std::uniform_real_distribution<float> alongDist(0.15f, 1.0f);
            std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * glm::pi<float>());
            std::uniform_real_distribution<float> tiltDist(-0.4f, 0.6f);
            std::uniform_real_distribution<float> scaleDist(0.75f, 1.30f);
            std::uniform_real_distribution<float> colorShift(-0.08f, 0.08f);

            glm::vec3 rawPerp = glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(rawPerp) < 0.001f) {
                rawPerp = glm::cross(direction, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            glm::vec3 twigPerp1 = glm::normalize(rawPerp);
            glm::vec3 twigPerp2 = glm::normalize(glm::cross(direction, twigPerp1));

            for (int i = 0; i < m_params.leavesPerTwig; ++i) {
                float t = alongDist(rng);
                float theta = angleDist(rng);
                glm::vec3 radialOut = std::cos(theta) * twigPerp1 + std::sin(theta) * twigPerp2;

                // Position the leaf slightly outside the twig surface
                float currentRadius = glm::mix(radius, endRadius, t);
                glm::vec3 leafPos = glm::mix(start, end, t) + radialOut * (currentRadius + 0.02f);

                // Point the leaf outward from the twig with a natural tilt
                glm::vec3 leafDir = glm::normalize(radialOut + direction * 0.4f + glm::vec3(0.0f, 0.0f, tiltDist(rng)));
                glm::vec3 leafRight = glm::normalize(glm::cross(leafDir, glm::vec3(0.0f, 0.0f, 1.0f) + radialOut * 0.2f));
                glm::vec3 leafNormal = glm::normalize(glm::cross(leafRight, leafDir));

                // Fresh spring/summer green with subtle per leaf variation
                float shift = colorShift(rng);
                glm::vec3 leafColor = glm::clamp(
                    glm::vec3(0.20f + shift, 0.62f + shift * 1.2f, 0.18f + shift * 0.5f),
                    glm::vec3(0.05f),
                    glm::vec3(0.95f)
                );

                m_leaves.push_back({
                    leafPos,
                    leafDir,
                    leafNormal,
                    m_params.leafScale * scaleDist(rng),
                    leafColor
                    });
            }
        }

        if (depth == 1) return;

        int childCount = (depth >= 4) ? 3 : 2;
        glm::vec3 splitPoint = end; // Branches grow continuously from the tip of the parent segment

        std::uniform_real_distribution<float> rotDist(0.0f, 2.0f * glm::pi<float>());
        std::uniform_real_distribution<float> lengthDist(0.80f, 0.95f);
        std::uniform_real_distribution<float> pitchJitter(-6.0f, 6.0f);

        for (int i = 0; i < childCount; ++i) {
            // Space child branches evenly around the circle with slight organic variation
            float baseAngle = (static_cast<float>(i) / static_cast<float>(childCount)) * 2.0f * glm::pi<float>();
            float angleRot = baseAngle + rotDist(rng) * 0.25f;
            float pitch = glm::radians(m_params.branchAngle + pitchJitter(rng));

            // Compute a vector perpendicular to the branch direction (check length before normalising)
            glm::vec3 rawPerp = glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(rawPerp) < 0.001f) {
                rawPerp = glm::cross(direction, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            glm::vec3 perp1 = glm::normalize(rawPerp);
            glm::vec3 perp2 = glm::normalize(glm::cross(direction, perp1));

            glm::vec3 childDir = glm::normalize(
                direction * std::cos(pitch) +
                (perp1 * std::cos(angleRot) + perp2 * std::sin(angleRot)) * std::sin(pitch)
            );

            float childLength = glm::length(end - start) * m_params.lengthShrink * lengthDist(rng);
            glm::vec3 childEnd = splitPoint + childDir * childLength;

            generateBranch(splitPoint, childEnd, endRadius, depth - 1, childDir, rng);
        }
    }
};