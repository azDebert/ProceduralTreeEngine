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
};

// Represents a single branch segment in 3D space
struct BranchSegment {
    glm::vec3 start;
    glm::vec3 end;
    float startRadius;
    float endRadius;
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

private:
    TreeParameters m_params;
    std::vector<BranchSegment> m_segments;

    void generate() {
        m_segments.clear();
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