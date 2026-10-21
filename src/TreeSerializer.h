#pragma once

#include "TreeModel.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

namespace TreeSerializer {

    // Clamp parameters to safe limits so bad files cannot crash the host engine
    inline TreeParameters sanitiseParameters(TreeParameters p) {
        auto safeFloat = [](float val, float fallback, float minVal, float maxVal) {
            if (!std::isfinite(val)) return fallback;
            return std::clamp(val, minVal, maxVal);
        };

        int speciesInt = static_cast<int>(p.species);
        if (speciesInt < 0 || speciesInt >= static_cast<int>(TreeSpecies::Count)) {
            p.species = TreeSpecies::EnglishOak;
        }

        p.trunkHeight    = safeFloat(p.trunkHeight,    2.0f, 0.5f, 10.0f);
        p.trunkRadius    = safeFloat(p.trunkRadius,    0.25f, 0.05f, 2.0f);
        p.recursionDepth = std::clamp(p.recursionDepth, 1, 7);
        p.branchAngle    = safeFloat(p.branchAngle,    35.0f, 5.0f, 85.0f);
        p.lengthShrink   = safeFloat(p.lengthShrink,   0.7f, 0.3f, 0.95f);
        p.radiusShrink   = safeFloat(p.radiusShrink,   0.6f, 0.3f, 0.95f);
        p.tropism        = safeFloat(p.tropism,        0.0f, -1.0f, 1.0f);
        p.radialSegments = std::clamp(p.radialSegments, 3, 32);
        p.leavesPerTwig  = std::clamp(p.leavesPerTwig,  0, 50);
        p.leafScale      = safeFloat(p.leafScale,      0.18f, 0.02f, 1.5f);

        p.barkColor.r    = safeFloat(p.barkColor.r,    0.42f, 0.0f, 1.0f);
        p.barkColor.g    = safeFloat(p.barkColor.g,    0.27f, 0.0f, 1.0f);
        p.barkColor.b    = safeFloat(p.barkColor.b,    0.14f, 0.0f, 1.0f);

        return p;
    }

    // Write tree parameters to a text string
    inline std::string serialiseToString(const TreeParameters& rawParams) {
        TreeParameters p = sanitiseParameters(rawParams);
        std::ostringstream out;
        out << "# ProceduralTreeEngine Preset v1.0\n";
        out << "seed=" << p.seed << "\n";
        out << "species=" << static_cast<int>(p.species) << "\n";
        out << "trunkHeight=" << p.trunkHeight << "\n";
        out << "trunkRadius=" << p.trunkRadius << "\n";
        out << "recursionDepth=" << p.recursionDepth << "\n";
        out << "branchAngle=" << p.branchAngle << "\n";
        out << "lengthShrink=" << p.lengthShrink << "\n";
        out << "radiusShrink=" << p.radiusShrink << "\n";
        out << "tropism=" << p.tropism << "\n";
        out << "barkColorR=" << p.barkColor.r << "\n";
        out << "barkColorG=" << p.barkColor.g << "\n";
        out << "barkColorB=" << p.barkColor.b << "\n";
        out << "radialSegments=" << p.radialSegments << "\n";
        out << "leavesPerTwig=" << p.leavesPerTwig << "\n";
        out << "leafScale=" << p.leafScale << "\n";
        return out.str();
    }

    // Read tree parameters from a text string with safe bounds checking
    inline bool deserialiseFromString(const std::string& content, TreeParameters& outParams) {
        if (content.empty() || content.size() > 65536) {
            return false;
        }

        TreeParameters p = getSpeciesPreset(TreeSpecies::EnglishOak, 42);
        std::istringstream in(content);
        std::string line;
        int validKeysRead = 0;

        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;

            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = line.substr(0, eqPos);
            std::string valStr = line.substr(eqPos + 1);

            try {
                if (key == "seed") {
                    p.seed = static_cast<uint32_t>(std::stoul(valStr));
                    validKeysRead++;
                } else if (key == "species") {
                    int sp = std::stoi(valStr);
                    sp = std::clamp(sp, 0, static_cast<int>(TreeSpecies::Count) - 1);
                    uint32_t currentSeed = p.seed;
                    p = getSpeciesPreset(static_cast<TreeSpecies>(sp), currentSeed);
                    validKeysRead++;
                } else if (key == "trunkHeight")    { p.trunkHeight = std::stof(valStr); validKeysRead++; }
                else if (key == "trunkRadius")      { p.trunkRadius = std::stof(valStr); validKeysRead++; }
                else if (key == "recursionDepth")   { p.recursionDepth = std::stoi(valStr); validKeysRead++; }
                else if (key == "branchAngle")      { p.branchAngle = std::stof(valStr); validKeysRead++; }
                else if (key == "lengthShrink")     { p.lengthShrink = std::stof(valStr); validKeysRead++; }
                else if (key == "radiusShrink")     { p.radiusShrink = std::stof(valStr); validKeysRead++; }
                else if (key == "tropism")          { p.tropism = std::stof(valStr); validKeysRead++; }
                else if (key == "barkColorR")       { p.barkColor.r = std::stof(valStr); validKeysRead++; }
                else if (key == "barkColorG")       { p.barkColor.g = std::stof(valStr); validKeysRead++; }
                else if (key == "barkColorB")       { p.barkColor.b = std::stof(valStr); validKeysRead++; }
                else if (key == "radialSegments")   { p.radialSegments = std::stoi(valStr); validKeysRead++; }
                else if (key == "leavesPerTwig")    { p.leavesPerTwig = std::stoi(valStr); validKeysRead++; }
                else if (key == "leafScale")        { p.leafScale = std::stof(valStr); validKeysRead++; }
            } catch (...) {
                // Ignore malformed numeric values and continue safely
                continue;
            }
        }

        if (validKeysRead == 0) {
            return false;
        }

        outParams = sanitiseParameters(p);
        return true;
    }

    inline bool savePresetToFile(const std::string& filePath, const TreeParameters& params) {
        std::ofstream file(filePath, std::ios::out | std::ios::trunc);
        if (!file.is_open()) return false;
        file << serialiseToString(params);
        return file.good();
    }

    inline bool loadPresetFromFile(const std::string& filePath, TreeParameters& outParams) {
        std::ifstream file(filePath, std::ios::in);
        if (!file.is_open()) return false;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return deserialiseFromString(buffer.str(), outParams);
    }

    // Export the generated tree to a standard Wavefront .obj file for Unreal, Unity, Godot, or Blender
    inline bool exportTreeToObj(const std::string& filePath, const TreeModel& tree, int radialSegments = 12) {
        std::ofstream out(filePath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) return false;

        radialSegments = std::clamp(radialSegments, 3, 32);
        out << "# Exported from ProceduralTreeEngine\n";
        out << "o Trunk_And_Branches\n";

        uint32_t vertexOffset = 1; // Wavefront .obj indices start at 1

        for (const auto& seg : tree.getSegments()) {
            glm::vec3 dir = seg.end - seg.start;
            float len = glm::length(dir);
            if (len < 0.0001f) continue;
            dir = glm::normalize(dir);

            glm::vec3 rawPerp = glm::cross(dir, glm::vec3(0.0f, 1.0f, 0.0f));
            if (glm::length(rawPerp) < 0.001f) {
                rawPerp = glm::cross(dir, glm::vec3(1.0f, 0.0f, 0.0f));
            }
            glm::vec3 perp1 = glm::normalize(rawPerp);
            glm::vec3 perp2 = glm::normalize(glm::cross(dir, perp1));

            for (int i = 0; i <= radialSegments; ++i) {
                float theta = static_cast<float>(i) / static_cast<float>(radialSegments) * 2.0f * glm::pi<float>();
                glm::vec3 normal = glm::normalize(perp1 * std::cos(theta) + perp2 * std::sin(theta));

                glm::vec3 vStart = seg.start + normal * seg.startRadius;
                glm::vec3 vEnd   = seg.end + normal * seg.endRadius;

                // Convert Z-up to standard Y-up for game engines
                out << "v " << vStart.x << " " << (vStart.z + 1.0f) << " " << vStart.y << "\n";
                out << "vn " << normal.x << " " << normal.z << " " << normal.y << "\n";

                out << "v " << vEnd.x << " " << (vEnd.z + 1.0f) << " " << vEnd.y << "\n";
                out << "vn " << normal.x << " " << normal.z << " " << normal.y << "\n";
            }

            for (int i = 0; i < radialSegments; ++i) {
                uint32_t s1 = vertexOffset + static_cast<uint32_t>(i * 2);
                uint32_t e1 = vertexOffset + static_cast<uint32_t>(i * 2 + 1);
                uint32_t s2 = vertexOffset + static_cast<uint32_t>((i + 1) * 2);
                uint32_t e2 = vertexOffset + static_cast<uint32_t>((i + 1) * 2 + 1);

                out << "f " << s1 << "//" << s1 << " " << s2 << "//" << s2 << " " << e1 << "//" << e1 << "\n";
                out << "f " << e1 << "//" << e1 << " " << s2 << "//" << s2 << " " << e2 << "//" << e2 << "\n";
            }

            vertexOffset += static_cast<uint32_t>((radialSegments + 1) * 2);
        }

        // Export leaf blades as a separate mesh group so game engines can assign a leaf material
        out << "o Foliage_Leaves\n";
        const glm::vec3 localDiamond[4] = {
            { 0.00f, 0.00f,  0.00f},
            {-0.35f, 0.45f,  0.05f},
            { 0.00f, 1.00f, -0.05f},
            { 0.35f, 0.45f,  0.05f}
        };

        for (const auto& leaf : tree.getLeaves()) {
            glm::vec3 forward = glm::normalize(leaf.direction);
            glm::vec3 up = glm::normalize(leaf.normal);
            glm::vec3 right = glm::normalize(glm::cross(forward, up));
            up = glm::cross(right, forward);

            for (const auto& corner : localDiamond) {
                glm::vec3 scaled = corner * leaf.scale;
                glm::vec3 p = leaf.position + right * scaled.x + forward * scaled.y + up * scaled.z;
                out << "v " << p.x << " " << (p.z + 1.0f) << " " << p.y << "\n";
                out << "vn " << up.x << " " << up.z << " " << up.y << "\n";
            }

            uint32_t b = vertexOffset;
            out << "f " << b << "//" << b << " " << (b + 1) << "//" << (b + 1) << " " << (b + 2) << "//" << (b + 2) << "\n";
            out << "f " << b << "//" << b << " " << (b + 2) << "//" << (b + 2) << " " << (b + 3) << "//" << (b + 3) << "\n";
            vertexOffset += 4;
        }

        return out.good();
    }

}