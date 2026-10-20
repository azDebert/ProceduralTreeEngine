#include "../src/TreeModel.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
    // Helper to check that a 3D vector contains valid finite numbers (no NaN or Infinity)
    bool isFiniteVec3(const glm::vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    bool testValidGeometry() {
        TreeParameters params{};
        params.seed = 42;
        params.recursionDepth = 5;

        TreeModel tree(params);
        const auto& segments = tree.getSegments();

        // Depth 5 with 1 trunk + 3*1 + 3*3 + 3*3*2 + 3*3*2*2 = 67 segments
        if (segments.empty()) {
            std::cerr << "[FAIL] testValidGeometry: Tree generated 0 segments.\n";
            return false;
        }

        for (size_t i = 0; i < segments.size(); ++i) {
            const auto& seg = segments[i];

            if (!isFiniteVec3(seg.start) || !isFiniteVec3(seg.end)) {
                std::cerr << "[FAIL] testValidGeometry: Segment " << i << " contains NaN or Inf coordinates.\n";
                return false;
            }

            float length = glm::length(seg.end - seg.start);
            if (length <= 0.0001f) {
                std::cerr << "[FAIL] testValidGeometry: Segment " << i << " has invalid length (" << length << ").\n";
                return false;
            }

            if (seg.startRadius <= 0.0f || seg.endRadius <= 0.0f || seg.endRadius > seg.startRadius) {
                std::cerr << "[FAIL] testValidGeometry: Segment " << i << " has invalid radii.\n";
                return false;
            }
        }

        std::cout << "[PASS] testValidGeometry (" << segments.size() << " valid segments verified)\n";

        const auto& leaves = tree.getLeaves();
        if (leaves.empty()) {
            std::cerr << "[FAIL] testValidGeometry: Tree generated 0 leaves.\n";
            return false;
        }
        for (size_t i = 0; i < leaves.size(); ++i) {
            if (!isFiniteVec3(leaves[i].position) || !isFiniteVec3(leaves[i].direction) ||
                !isFiniteVec3(leaves[i].normal) || leaves[i].scale <= 0.0f) {
                std::cerr << "[FAIL] testValidGeometry: Leaf " << i << " contains invalid data.\n";
                return false;
            }
        }
        std::cout << "[PASS] testValidLeaves (" << leaves.size() << " valid leaves verified)\n";

        return true;
    }

    bool testDeterministicSeeds() {
        TreeParameters paramsA{};
        paramsA.seed = 1337;

        TreeParameters paramsB{};
        paramsB.seed = 1337;

        TreeModel treeA(paramsA);
        TreeModel treeB(paramsB);

        const auto& segsA = treeA.getSegments();
        const auto& segsB = treeB.getSegments();

        if (segsA.size() != segsB.size()) {
            std::cerr << "[FAIL] testDeterministicSeeds: Segment counts differ for identical seeds.\n";
            return false;
        }

        for (size_t i = 0; i < segsA.size(); ++i) {
            if (segsA[i].start != segsB[i].start ||
                segsA[i].end != segsB[i].end ||
                segsA[i].startRadius != segsB[i].startRadius ||
                segsA[i].endRadius != segsB[i].endRadius) {
                std::cerr << "[FAIL] testDeterministicSeeds: Segment " << i << " differs for identical seeds.\n";
                return false;
            }
        }

        // Verify that a different seed produces different branch positions
        TreeParameters paramsC{};
        paramsC.seed = 9999;
        TreeModel treeC(paramsC);
        const auto& segsC = treeC.getSegments();

        bool foundDifference = false;
        for (size_t i = 0; i < std::min(segsA.size(), segsC.size()); ++i) {
            if (segsA[i].end != segsC[i].end) {
                foundDifference = true;
                break;
            }
        }

        if (!foundDifference) {
            std::cerr << "[FAIL] testDeterministicSeeds: Different seeds produced identical trees.\n";
            return false;
        }

        // Verify that changing leaf density does NOT alter the branch skeleton
        TreeParameters paramsLowLeaves{};
        paramsLowLeaves.seed = 42;
        paramsLowLeaves.leavesPerTwig = 2;

        TreeParameters paramsHighLeaves{};
        paramsHighLeaves.seed = 42;
        paramsHighLeaves.leavesPerTwig = 30;

        TreeModel treeLow(paramsLowLeaves);
        TreeModel treeHigh(paramsHighLeaves);

        const auto& segsLow = treeLow.getSegments();
        const auto& segsHigh = treeHigh.getSegments();

        if (segsLow.size() != segsHigh.size()) {
            std::cerr << "[FAIL] testDeterministicSeeds: Leaf density altered branch count!\n";
            return false;
        }

        for (size_t i = 0; i < segsLow.size(); ++i) {
            if (segsLow[i].start != segsHigh[i].start || segsLow[i].end != segsHigh[i].end) {
                std::cerr << "[FAIL] testDeterministicSeeds: Leaf density altered branch geometry at segment " << i << "!\n";
                return false;
            }
        }

        std::cout << "[PASS] testDeterministicSeeds (Repeatability and seed variation verified)\n";
        return true;
    }
}

int main() {
    std::cout << "Running Procedural Tree Model unit tests...\n";

    bool allPassed = true;
    allPassed = testValidGeometry() && allPassed;
    allPassed = testDeterministicSeeds() && allPassed;

    if (!allPassed) {
        std::cerr << "Some tests FAILED!\n";
        return EXIT_FAILURE;
    }

    std::cout << "All TreeModel tests PASSED successfully!\n";
    return EXIT_SUCCESS;
}