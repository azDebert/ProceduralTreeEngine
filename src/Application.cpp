#include "Application.h"
#include "TreeSerializer.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <set>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    // Allow CPU to work on 2 frames ahead of the GPU
    constexpr int kMaxFramesInFlight = 2;

    // Builds the 5 species-specific 3D foliage meshes + 1 wind streak mesh into a single vertex/index catalogue
    struct FoliageMeshCatalogue {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::array<FoliageDrawBatch, 6> meshRanges{};
    };

    FoliageMeshCatalogue buildFoliageMeshCatalogue() {
        FoliageMeshCatalogue cat;

        auto appendMesh = [&](int typeIdx, const std::vector<Vertex>& verts, const std::vector<uint32_t>& inds) {
            FoliageDrawBatch batch{};
            batch.vertexOffset = static_cast<int32_t>(cat.vertices.size());
            batch.firstIndex = static_cast<uint32_t>(cat.indices.size());
            batch.indexCount = static_cast<uint32_t>(inds.size());
            cat.meshRanges[typeIdx] = batch;

            cat.vertices.insert(cat.vertices.end(), verts.begin(), verts.end());
            cat.indices.insert(cat.indices.end(), inds.begin(), inds.end());
            };

        // 0: English Oak — 9-vertex V-folded lobed broadleaf
        appendMesh(0, {
            { { 0.00f, 0.00f,  0.00f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f }, // 0: Stem base
            { { 0.00f, 0.45f, -0.04f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f }, // 1: Midrib centre crease
            { { 0.00f, 1.02f, -0.08f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f }, // 2: Rounded apex tip
            { { -0.32f, 0.25f,  0.06f },{ 1.0f, 1.0f, 1.0f },{ -0.35f, 0.0f, 0.94f }, 0.0f }, // 3: Lower-left lobe
            { { -0.18f, 0.48f,  0.02f },{ 1.0f, 1.0f, 1.0f },{ -0.35f, 0.0f, 0.94f }, 0.0f }, // 4: Left sinus indentation
            { { -0.44f, 0.72f,  0.08f },{ 1.0f, 1.0f, 1.0f },{ -0.35f, 0.0f, 0.94f }, 0.0f }, // 5: Upper-left wide lobe
            { { 0.32f, 0.25f,  0.06f },{ 1.0f, 1.0f, 1.0f },{ 0.35f, 0.0f, 0.94f }, 0.0f }, // 6: Lower-right lobe
            { { 0.18f, 0.48f,  0.02f },{ 1.0f, 1.0f, 1.0f },{ 0.35f, 0.0f, 0.94f }, 0.0f }, // 7: Right sinus indentation
            { { 0.44f, 0.72f,  0.08f },{ 1.0f, 1.0f, 1.0f },{ 0.35f, 0.0f, 0.94f }, 0.0f }  // 8: Upper-right wide lobe
            }, {
                0, 3, 1,  3, 4, 1,  4, 5, 1,  5, 2, 1,
                0, 1, 6,  1, 7, 6,  1, 8, 7,  1, 2, 8
            });

        // 1: Japanese Sakura — 7-vertex cupped blossom / folded ovate leaf
        appendMesh(1, {
            { { 0.00f, 0.00f,  0.00f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f },
            { { 0.00f, 0.50f, -0.05f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f },
            { { -0.38f, 0.32f,  0.07f },{ 1.0f, 1.0f, 1.0f },{ -0.30f, 0.0f, 0.95f }, 0.0f },
            { { -0.34f, 0.78f,  0.06f },{ 1.0f, 1.0f, 1.0f },{ -0.30f, 0.0f, 0.95f }, 0.0f },
            { { 0.00f, 1.04f,  0.02f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f },
            { { 0.34f, 0.78f,  0.06f },{ 1.0f, 1.0f, 1.0f },{ 0.30f, 0.0f, 0.95f }, 0.0f },
            { { 0.38f, 0.32f,  0.07f },{ 1.0f, 1.0f, 1.0f },{ 0.30f, 0.0f, 0.95f }, 0.0f }
            }, {
                0, 2, 1,  2, 3, 1,  3, 4, 1,
                0, 1, 6,  1, 5, 6,  1, 4, 5
            });

         // 2: Silver Birch — 6-vertex folded deltoid (triangular-diamond) leaf with arched tip
         appendMesh(2, {
            { { 0.00f, 0.00f,  0.02f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f },
            { { 0.00f, 0.42f,  0.00f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f },
            { { 0.00f, 1.05f, -0.12f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.2f, 0.98f }, 0.0f },
            { { -0.42f, 0.28f,  0.06f },{ 1.0f, 1.0f, 1.0f },{ -0.35f, 0.0f, 0.94f }, 0.0f },
            { { 0.42f, 0.28f,  0.06f },{ 1.0f, 1.0f, 1.0f },{ 0.35f, 0.0f, 0.94f }, 0.0f },
            { { 0.00f, 0.72f, -0.04f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f }
            }, {
                0, 3, 1,  3, 2, 1,
                0, 1, 4,  1, 2, 4
            });

          // 3: Weeping Willow — 7-vertex long slender curved lanceolate blade (5:1 length-to-width)
          appendMesh(3, {
            { { 0.00f, 0.00f,  0.00f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f },
            { { -0.14f, 0.35f,  0.03f },{ 1.0f, 1.0f, 1.0f },{ -0.25f, 0.0f, 0.97f }, 0.0f },
            { { 0.14f, 0.35f,  0.03f },{ 1.0f, 1.0f, 1.0f },{ 0.25f, 0.0f, 0.97f }, 0.0f },
            { { 0.00f, 0.55f, -0.02f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.0f, 1.00f }, 0.0f },
            { { -0.12f, 0.95f, -0.08f },{ 1.0f, 1.0f, 1.0f },{ -0.25f, 0.0f, 0.97f }, 0.0f },
            { { 0.12f, 0.95f, -0.08f },{ 1.0f, 1.0f, 1.0f },{ 0.25f, 0.0f, 0.97f }, 0.0f },
            { { 0.00f, 1.55f, -0.22f },{ 1.0f, 1.0f, 1.0f },{ 0.00f, 0.2f, 0.98f }, 0.0f }
            }, {
                0, 1, 3,  0, 3, 2,
                1, 4, 3,  3, 5, 2,
                4, 6, 3,  3, 6, 5
            });

           // 4: Scots Pine — 18-vertex 3D starburst fascicle of 6 crossed slender pine needles
           std::vector<Vertex> pineVerts;
           std::vector<uint32_t> pineInds;
           const glm::vec3 needleDirs[6] = {
                glm::normalize(glm::vec3(0.45f, 0.85f,  0.35f)),
                glm::normalize(glm::vec3(-0.45f, 0.85f,  0.35f)),
                glm::normalize(glm::vec3(0.00f, 0.92f,  0.55f)),
                glm::normalize(glm::vec3(0.55f, 0.70f, -0.25f)),
                glm::normalize(glm::vec3(-0.55f, 0.70f, -0.25f)),
                glm::normalize(glm::vec3(0.00f, 0.80f, -0.45f))
           };
           for (int n = 0; n < 6; ++n) {
               uint32_t b = static_cast<uint32_t>(pineVerts.size());
               glm::vec3 d = needleDirs[n];
               glm::vec3 side = glm::normalize(glm::cross(d, glm::vec3(0.0f, 0.0f, 1.0f)));
               glm::vec3 norm = glm::normalize(glm::cross(side, d));
               float w = 0.045f; // Very thin pine needle width
               pineVerts.push_back({ -side * w,{ 1.0f, 1.0f, 1.0f }, norm, 0.0f });
               pineVerts.push_back({ side * w,{ 1.0f, 1.0f, 1.0f }, norm, 0.0f });
               pineVerts.push_back({ d * 1.15f,{ 1.0f, 1.0f, 1.0f }, norm, 0.0f });
               pineInds.push_back(b);
               pineInds.push_back(b + 1);
               pineInds.push_back(b + 2);
           }
           appendMesh(4, pineVerts, pineInds);
           
           // 5: Wind Streak Ribbon — 4-vertex elongated ribbon
           appendMesh(5, {
               { { 0.0f, -0.5f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f }, 0.0f },
               { { -0.5f,  0.0f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f }, 0.0f },
               { { 0.0f,  0.5f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f }, 0.0f },
               { { 0.5f,  0.0f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f }, 0.0f }
               }, { 0, 1, 2,  0, 2, 3 });
           return cat;
    }

    struct SceneGeometry {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<LeafInstance> leaves;
        std::vector<FoliageDrawBatch> batches;
    };

    SceneGeometry buildSceneGeometry(const TreeParameters& baseParams, bool forestGrove, const FoliageMeshCatalogue& catalogue) {
        SceneGeometry scene;

        // Octagonal ground plane at Z = -1.0f
        const float groundRadius = forestGrove ? 8.0f : 3.8f;
        const int groundSides = 24;
        const glm::vec3 groundColor(0.22f, 0.28f, 0.18f);
        const glm::vec3 upNormal(0.0f, 0.0f, 1.0f);

        uint32_t centerIdx = static_cast<uint32_t>(scene.vertices.size());
        scene.vertices.push_back({ { 0.0f, 0.0f, -1.0f }, groundColor, upNormal, 0.0f });

        for (int i = 0; i <= groundSides; ++i) {
            float angle = static_cast<float>(i) / static_cast<float>(groundSides) * 2.0f * glm::pi<float>();
            scene.vertices.push_back({
                { std::cos(angle) * groundRadius, std::sin(angle) * groundRadius, -1.0f },
                groundColor * 0.85f,
                upNormal,
                0.0f
                });
        }

        for (int i = 0; i < groundSides; ++i) {
            scene.indices.push_back(centerIdx);
            scene.indices.push_back(centerIdx + 1 + static_cast<uint32_t>(i));
            scene.indices.push_back(centerIdx + 1 + static_cast<uint32_t>(i + 1));
        }

        // Add 3D Wind Compass Ring & Rotating Arrow at (0.0, -3.1, -0.97)
        const glm::vec2 compassCenter(0.0f, -3.1f);
        const float compassZ = -0.97f;
        uint32_t arrowBase = static_cast<uint32_t>(scene.vertices.size());
        const glm::vec3 arrowColor(0.95f, 0.38f, 0.15f);
        const glm::vec3 tailColor(0.25f, 0.65f, 0.92f);

        // Arrow shaft & head pointing along +X in local space (rotated by ubo.windDir in vertex shader via flex = -1.0f)
        scene.vertices.push_back({ { compassCenter.x - 0.42f, compassCenter.y - 0.05f, compassZ }, tailColor,  upNormal, -1.0f });
        scene.vertices.push_back({ { compassCenter.x + 0.15f, compassCenter.y - 0.05f, compassZ }, arrowColor, upNormal, -1.0f });
        scene.vertices.push_back({ { compassCenter.x + 0.15f, compassCenter.y + 0.05f, compassZ }, arrowColor, upNormal, -1.0f });
        scene.vertices.push_back({ { compassCenter.x - 0.42f, compassCenter.y + 0.05f, compassZ }, tailColor,  upNormal, -1.0f });
        scene.vertices.push_back({ { compassCenter.x + 0.15f, compassCenter.y - 0.18f, compassZ }, arrowColor, upNormal, -1.0f });
        scene.vertices.push_back({ { compassCenter.x + 0.52f, compassCenter.y,         compassZ }, arrowColor, upNormal, -1.0f });
        scene.vertices.push_back({ { compassCenter.x + 0.15f, compassCenter.y + 0.18f, compassZ }, arrowColor, upNormal, -1.0f });

        uint32_t arrowInds[] = { 0, 1, 2,  0, 2, 3,  4, 5, 6 };
        for (uint32_t idx : arrowInds) {
            scene.indices.push_back(arrowBase + idx);
        }

        // Generate 1 tree or a 5-species grove
        std::vector<glm::vec3> treeOffsets = { glm::vec3(0.0f, 0.0f, 0.0f) };
        if (forestGrove) {
            treeOffsets.push_back(glm::vec3(-3.4f, -1.8f, 0.0f));
            treeOffsets.push_back(glm::vec3(3.4f, -1.6f, 0.0f));
            treeOffsets.push_back(glm::vec3(-2.6f, 2.9f, 0.0f));
            treeOffsets.push_back(glm::vec3(2.8f, 2.7f, 0.0f));
        }

        // Calculate proportional adjustments from baseParams so [- / =] and [ / ] update ALL trees in the grove
        TreeParameters defaultActivePreset = getSpeciesPreset(baseParams.species, baseParams.seed);
        float leafDensityScale = (defaultActivePreset.leavesPerTwig > 0)
            ? static_cast<float>(baseParams.leavesPerTwig) / static_cast<float>(defaultActivePreset.leavesPerTwig)
            : 1.0f;
        float branchAngleDelta = baseParams.branchAngle - defaultActivePreset.branchAngle;

        std::array<std::vector<LeafInstance>, 6> leavesBySpecies;

        for (size_t t = 0; t < treeOffsets.size(); ++t) {
            TreeParameters params = baseParams;
            if (forestGrove && t > 0) {
                TreeSpecies sp = static_cast<TreeSpecies>(t % static_cast<size_t>(TreeSpecies::Count));
                params = getSpeciesPreset(sp, baseParams.seed + static_cast<uint32_t>(t * 101));
                params.leavesPerTwig = std::clamp(
                    static_cast<int>(std::round(static_cast<float>(params.leavesPerTwig) * leafDensityScale)),
                    0, 50
                );
                params.branchAngle = std::clamp(params.branchAngle + branchAngleDelta, 10.0f, 80.0f);
            }

            int radialSegments = params.radialSegments;
            TreeModel tree(params);
            glm::vec3 worldOffset = treeOffsets[t];

            for (const auto& seg : tree.getSegments()) {
                glm::vec3 dir = seg.end - seg.start;
                float length = glm::length(dir);
                if (length < 0.0001f) continue;
                dir = glm::normalize(dir);

                glm::vec3 rawPerp = glm::cross(dir, glm::vec3(0.0f, 1.0f, 0.0f));
                if (glm::length(rawPerp) < 0.001f) {
                    rawPerp = glm::cross(dir, glm::vec3(1.0f, 0.0f, 0.0f));
                }
                glm::vec3 perp1 = glm::normalize(rawPerp);
                glm::vec3 perp2 = glm::normalize(glm::cross(dir, perp1));

                uint32_t baseIndex = static_cast<uint32_t>(scene.vertices.size());

                for (int i = 0; i <= radialSegments; ++i) {
                    float theta = static_cast<float>(i) / static_cast<float>(radialSegments) * 2.0f * glm::pi<float>();
                    glm::vec3 normal = glm::normalize(perp1 * std::cos(theta) + perp2 * std::sin(theta));

                    glm::vec3 startBark = seg.barkColor * 0.90f;
                    glm::vec3 endBark = seg.barkColor * 1.08f;
                    if (params.species == TreeSpecies::SilverBirch && std::sin(seg.start.z * 14.0f) > 0.65f) {
                        startBark *= 0.32f;
                    }

                    scene.vertices.push_back({
                        seg.start + worldOffset + normal * seg.startRadius,
                        startBark,
                        normal,
                        seg.startFlex
                        });

                    scene.vertices.push_back({
                        seg.end + worldOffset + normal * seg.endRadius,
                        endBark,
                        normal,
                        seg.endFlex
                        });
                }

                for (int i = 0; i < radialSegments; ++i) {
                    uint32_t start1 = baseIndex + static_cast<uint32_t>(i * 2);
                    uint32_t end1 = baseIndex + static_cast<uint32_t>(i * 2 + 1);
                    uint32_t start2 = baseIndex + static_cast<uint32_t>((i + 1) * 2);
                    uint32_t end2 = baseIndex + static_cast<uint32_t>((i + 1) * 2 + 1);

                    scene.indices.push_back(start1);
                    scene.indices.push_back(start2);
                    scene.indices.push_back(end1);

                    scene.indices.push_back(end1);
                    scene.indices.push_back(start2);
                    scene.indices.push_back(end2);
                }
            }

            // Stagger tree seasonal phase slightly in grove mode so some trees shift colour earlier than others
            float treePhaseShift = static_cast<float>(t) * 0.16f;
            for (auto leaf : tree.getLeaves()) {
                leaf.position += worldOffset;
                leaf.phase = std::fmod(leaf.phase + treePhaseShift, 0.99f);
                int bucket = std::clamp(leaf.speciesType, 0, 4);
                leavesBySpecies[bucket].push_back(leaf);
            }
        }

        // Add 28 visible 3D wind streak ribbons across the airspace
        std::mt19937 windRng(1337);
        std::uniform_real_distribution<float> xyDist(-4.5f, 4.5f);
        std::uniform_real_distribution<float> zDist(0.2f, 3.4f);
        std::uniform_real_distribution<float> pDist(0.0f, 1.0f);
        for (int i = 0; i < 28; ++i) {
            leavesBySpecies[5].push_back({
                glm::vec3(xyDist(windRng), xyDist(windRng), zDist(windRng)),
                glm::vec3(1.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 0.0f, 1.0f),
                1.0f,
                glm::vec3(0.85f, 0.92f, 1.0f),
                pDist(windRng),
                5,
                0.0f
                });
        }

        // Pack sorted instances into scene.leaves and record per-species draw batches
        for (size_t s = 0; s < leavesBySpecies.size(); ++s) {
            if (leavesBySpecies[s].empty()) continue;
            FoliageDrawBatch batch = catalogue.meshRanges[s];
            batch.firstInstance = static_cast<uint32_t>(scene.leaves.size());
            batch.instanceCount = static_cast<uint32_t>(leavesBySpecies[s].size());
            scene.batches.push_back(batch);
            scene.leaves.insert(scene.leaves.end(), leavesBySpecies[s].begin(), leavesBySpecies[s].end());
        }

        return scene;
    }

    // Validation layer used for catching Vulkan bugs
    const std::vector<const char*> kValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Swapchain device extension must be enabled on GPU
    const std::vector<const char*> kDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // NDEBUG is defined automatically by CMake in Release builds.
    // So validation layers are ON in Debug builds, and OFF in Release builds.
#ifdef NDEBUG
    constexpr bool kEnableValidationLayers = false;
#else
    constexpr bool kEnableValidationLayers = true;
#endif
}

Application::Application(uint32_t width, uint32_t height, const std::string& title)
    : m_width(width), m_height(height), m_title(title) {
    initWindow();
    initVulkan();
}

Application::~Application() {
    cleanup();
}

void Application::run() {
    mainLoop();
}

void Application::initWindow() {
    // Initialise the GLFW library
    if (glfwInit() == GLFW_FALSE) {
        throw std::runtime_error("Failed to initialise GLFW!");
    }

    // Tell GLFW NOT to create an OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Create the window
    m_window = glfwCreateWindow(
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        m_title.c_str(),
        nullptr,
        nullptr
    );

    if (m_window == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window!");
    }

    // Store pointer to this Application and register resize callback
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);

    std::cout << "GLFW Window (" << m_width << "x" << m_height << ") initialised successfully.\n";
}

void Application::initVulkan() {
	createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createDepthResources();
    createFramebuffers();
    createSceneBuffers();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

void Application::createInstance() {
    // Make sure validation layers are installed before asking for them
    if (kEnableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    // Optional info about the app for the GPU driver
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_title.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    // Main struct telling Vulkan which extensions and layers to load
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Ask GLFW which Vulkan extensions it needs to draw in a window on this OS
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    // Turn on validation layers if we are in Debug mode
    if (kEnableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    // Create the Vulkan instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }

    std::cout << "Vulkan 1.4 instance created (Validation Layers: "
        << (kEnableValidationLayers ? "ON" : "OFF") << ").\n";
}

bool Application::checkValidationLayerSupport() const {
    // First call gets the number of installed layers
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    // Second call fills our vector with the actual layer details
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // Check if every layer we want is in the installed list
    for (const char* layerName : kValidationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

void Application::createSurface() {
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }
}

void Application::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find any GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Prefer a dedicated (discrete) GPU like the RTX 5070 Ti over integrated graphics
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);

        if (isDeviceSuitable(device) && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_physicalDevice = device;
            std::cout << "Selected Discrete GPU: " << props.deviceName << '\n';
            break;
        }
    }

    // Fallback to any suitable GPU if no discrete card was found
    if (m_physicalDevice == VK_NULL_HANDLE) {
        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(device, &props);
                m_physicalDevice = device;
                std::cout << "Selected Fallback GPU: " << props.deviceName << '\n';
                break;
            }
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

bool Application::isDeviceSuitable(VkPhysicalDevice device) const {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    // Check that the swapchain supports at least one image format and present mode
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool Application::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices Application::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // Check if this queue family can present images to the window surface
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

SwapChainSupportDetails Application::querySwapChainSupport(VkPhysicalDevice device) const {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR Application::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const {
    // Prefer 32-bit BGRA with sRGB colour space
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR Application::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const {
    // Prefer Mailbox (triple buffering) if available, otherwise fallback to FIFO (V-Sync)
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Application::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

void Application::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    // Use a set so if graphics and present are the same queue index, only request it once
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    // Enable the swapchain extension on the logical device
    createInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
    std::cout << "Logical device and queues created.\n";
}

void Application::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // Request 1 more image than the minimum so it doesn't stall waiting on the driver
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain!");
    }

    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());

    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;

    std::cout << "Swapchain created (" << extent.width << "x" << extent.height
        << ", " << imageCount << " images).\n";
}

void Application::createImageViews() {
    m_swapChainImageViews.resize(m_swapChainImages.size());

    for (size_t i = 0; i < m_swapChainImages.size(); ++i) {
        m_swapChainImageViews[i] = createImageView(
            m_swapChainImages[i],
            m_swapChainImageFormat,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    }
    std::cout << "Created " << m_swapChainImageViews.size() << " swapchain image views.\n";
}

void Application::createRenderPass() {
    // Describe the single colour attachment (the swapchain image)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // Clear screen before drawing
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Keep drawn pixels for display
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Ready for monitor presentation

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Describe the depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear depth to 1.0 before drawing
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Wait until the swapchain finishes acquiring the image before clearing/writing to it
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;

    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();

    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}

void Application::createDescriptorSetLayout() {
    // Describe binding 0: a Uniform Buffer Object accessible in the vertex shader
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

void Application::createGraphicsPipeline() {
    auto vertShaderCode = readFile("build/Debug/shaders/triangle.vert.spv");
    auto fragShaderCode = readFile("build/Debug/shaders/triangle.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // Bind the C++ Vertex and LeafInstance struct layouts to the pipeline
    auto bindingDescriptions = Vertex::getBindingDescriptions();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Draw standard triangles from every 3 vertices
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Allow viewport and scissor rectangle to be resized dynamically without rebuilding the pipeline
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasteriser: turns triangles into pixels
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling (anti-aliasing) off for now
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth testing: keep fragments that are closer to the camera (less than existing depth)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Colour blending: overwrite background colour with the fragment shader output
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Pipeline layout (holds uniform variables/push constants later)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;

    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    // Once the pipeline is baked, there is no need forthe temporary shader module wrappers anymore
    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);

    std::cout << "Render pass and graphics pipeline created successfully.\n";
}

VkShaderModule Application::createShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }
    return shaderModule;
}

std::vector<char> Application::readFile(const std::string& filename) {
    // std::ios::ate starts reading at the end to immediately know the file size
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    return buffer;
}

void Application::createFramebuffers() {
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

    for (size_t i = 0; i < m_swapChainImageViews.size(); ++i) {
        std::array<VkImageView, 2> attachments = {
                    m_swapChainImageViews[i],
                    m_depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_swapChainExtent.width;
        framebufferInfo.height = m_swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

void Application::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // Allow individual command buffers to be re-recorded every frame
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }
}

void Application::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    createImage(
        m_swapChainExtent.width,
        m_swapChainExtent.height,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_depthImage,
        m_depthImageMemory
    );

    m_depthImageView = createImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VkFormat Application::findSupportedFormat(
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
) const {
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported Vulkan format!");
}

VkFormat Application::findDepthFormat() const {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

void Application::createImage(
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage& image,
    VkDeviceMemory& imageMemory
) const {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(m_device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(m_device, image, imageMemory, 0);
}

VkImageView Application::createImageView(
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags
) const {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view!");
    }

    return imageView;
}

void Application::createSceneBuffers() {
    auto uploadVectorToBuffer = [this](const auto& vec, VkBufferUsageFlags usage, VkBuffer& dstBuffer, VkDeviceMemory& dstMemory) {
        VkDeviceSize size = sizeof(vec[0]) * vec.size();
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

        createBuffer(
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingMemory
        );

        void* data = nullptr;
        vkMapMemory(m_device, stagingMemory, 0, size, 0, &data);
        std::memcpy(data, vec.data(), static_cast<size_t>(size));
        vkUnmapMemory(m_device, stagingMemory);

        createBuffer(
            size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            dstBuffer,
            dstMemory
        );

        copyBuffer(stagingBuffer, dstBuffer, size);

        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
        };

    FoliageMeshCatalogue catalogue = buildFoliageMeshCatalogue();
    SceneGeometry scene = buildSceneGeometry(m_treeParams, m_forestGroveMode, catalogue);
    m_indexCount = static_cast<uint32_t>(scene.indices.size());
    m_leafInstanceCount = static_cast<uint32_t>(scene.leaves.size());
    m_foliageBatches = scene.batches;

    uploadVectorToBuffer(scene.vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vertexBuffer, m_vertexBufferMemory);
    uploadVectorToBuffer(scene.indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_indexBuffer, m_indexBufferMemory);

    std::vector<LeafInstance> trunkDummyInstance = {
        { glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), 0.0f, glm::vec3(1.0f), 0.0f, 0, 0.0f }
    };
    uploadVectorToBuffer(trunkDummyInstance, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_trunkInstanceBuffer, m_trunkInstanceBufferMemory);

    uploadVectorToBuffer(catalogue.vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_leafVertexBuffer, m_leafVertexBufferMemory);
    uploadVectorToBuffer(catalogue.indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_leafIndexBuffer, m_leafIndexBufferMemory);

    if (m_leafInstanceCount > 0) {
        uploadVectorToBuffer(scene.leaves, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_leafInstanceBuffer, m_leafInstanceBufferMemory);
    }

    std::cout << "Scene built: " << (m_forestGroveMode ? 5 : 1) << " tree(s) | "
        << scene.vertices.size() << " branch verts | "
        << m_leafInstanceCount << " 3D foliage instances | Seed: " << m_treeParams.seed << '\n';
}


void Application::destroySceneBuffers() {
    auto destroyBufferPair = [this](VkBuffer& buf, VkDeviceMemory& mem) {
        if (buf != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, buf, nullptr);
            buf = VK_NULL_HANDLE;
        }
        if (mem != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, mem, nullptr);
            mem = VK_NULL_HANDLE;
        }
        };

    destroyBufferPair(m_leafInstanceBuffer, m_leafInstanceBufferMemory);
    destroyBufferPair(m_leafIndexBuffer, m_leafIndexBufferMemory);
    destroyBufferPair(m_leafVertexBuffer, m_leafVertexBufferMemory);
    destroyBufferPair(m_trunkInstanceBuffer, m_trunkInstanceBufferMemory);
    destroyBufferPair(m_indexBuffer, m_indexBufferMemory);
    destroyBufferPair(m_vertexBuffer, m_vertexBufferMemory);
}

void Application::rebuildSceneBuffers() {
    vkDeviceWaitIdle(m_device);
    destroySceneBuffers();
    createSceneBuffers();
}

void Application::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    m_uniformBuffers.resize(kMaxFramesInFlight);
    m_uniformBuffersMemory.resize(kMaxFramesInFlight);
    m_uniformBuffersMapped.resize(kMaxFramesInFlight);

    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            m_uniformBuffers[i],
            m_uniformBuffersMemory[i]
        );

        vkMapMemory(m_device, m_uniformBuffersMemory[i], 0, bufferSize, 0, &m_uniformBuffersMapped[i]);
    }
}

void Application::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(kMaxFramesInFlight);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(kMaxFramesInFlight);

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }
}

void Application::createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(kMaxFramesInFlight, m_descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(kMaxFramesInFlight);
    allocInfo.pSetLayouts = layouts.data();

    m_descriptorSets.resize(kMaxFramesInFlight);
    if (vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
    }
}

void Application::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
) const {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements{};
    vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(m_device, buffer, bufferMemory, 0);
}

void Application::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
}

uint32_t Application::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void Application::createCommandBuffers() {
    m_commandBuffers.resize(kMaxFramesInFlight);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

void Application::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) const {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { { 0.08f, 0.11f, 0.16f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChainExtent.width);
    viewport.height = static_cast<float>(m_swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayout,
        0, 1,
        &m_descriptorSets[m_currentFrame],
        0, nullptr
    );

    // Draw the ground plane, 3D wind compass arrow, and tree branch skeletons
    VkBuffer trunkBuffers[] = { m_vertexBuffer, m_trunkInstanceBuffer };
    VkDeviceSize offsets[] = { 0, 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, trunkBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);

    // Draw each species' 3D foliage mesh (lobed oak leaf, sakura blossom, birch leaf, willow blade, 3D pine needles, wind streaks)
    if (m_leafInstanceCount > 0 && !m_foliageBatches.empty()) {
        VkBuffer leafBuffers[] = { m_leafVertexBuffer, m_leafInstanceBuffer };
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, leafBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_leafIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

        for (const auto& batch : m_foliageBatches) {
            vkCmdDrawIndexed(
                commandBuffer,
                batch.indexCount,
                batch.instanceCount,
                batch.firstIndex,
                batch.vertexOffset,
                batch.firstInstance
            );
        }
    }

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

void Application::createSyncObjects() {
    m_imageAvailableSemaphores.resize(kMaxFramesInFlight);
    m_renderFinishedSemaphores.resize(m_swapChainImages.size());
    m_inFlightFences.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Start fences in the signalled state so the very first frame doesn't wait forever
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create frame synchronisation objects!");
        }
    }

    for (size_t i = 0; i < m_swapChainImages.size(); ++i) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render-finished semaphores!");
        }
    }
}

void Application::updateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    static auto lastFrameTime = startTime;

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastFrameTime).count();
    lastFrameTime = currentTime;

    processInput(deltaTime);

    if (m_autoCycleSeasons) {
        m_season = std::fmod(m_season + deltaTime * 0.22f, 4.0f);
    }
    if (m_autoOrbitCamera) {
        m_cameraYaw += deltaTime * 25.0f;
    }

    m_frameCount++;
    m_fpsTimer += deltaTime;
    if (m_fpsTimer >= 0.5f) {
        float fps = static_cast<float>(m_frameCount) / m_fpsTimer;
        const char* seasonNames[] = { "Spring", "Summer", "Autumn", "Winter" };
        int seasonIdx = static_cast<int>(std::floor(m_season)) % 4;

        const char* compassDirs[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
        int compassIdx = static_cast<int>(std::round(m_windAngleDeg / 45.0f)) % 8;
        if (compassIdx < 0) compassIdx += 8;

        std::string title = m_treeParams.speciesName + " | " + seasonNames[seasonIdx]
            + " | Wind: " + compassDirs[compassIdx] + " (" + std::to_string(static_cast<int>(m_windAngleDeg)) + " deg)"
            + " | Leaves: " + std::to_string(m_leafInstanceCount)
            + " | " + std::to_string(static_cast<int>(fps)) + " FPS";
        glfwSetWindowTitle(m_window, title.c_str());

        m_frameCount = 0;
        m_fpsTimer = 0.0f;
    }

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);

    float yawRad = glm::radians(m_cameraYaw);
    float pitchRad = glm::radians(m_cameraPitch);
    glm::vec3 target(0.0f, 0.0f, m_cameraTargetZ);
    glm::vec3 eye(
        target.x + m_cameraDistance * std::cos(pitchRad) * std::sin(yawRad),
        target.y - m_cameraDistance * std::cos(pitchRad) * std::cos(yawRad),
        target.z + m_cameraDistance * std::sin(pitchRad)
    );

    ubo.view = glm::lookAt(eye, target, glm::vec3(0.0f, 0.0f, 1.0f));

    float aspectRatio = static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);
    ubo.proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 50.0f);
    ubo.proj[1][1] *= -1.0f;

    ubo.season = m_season;
    ubo.time = time;
    ubo.windStrength = m_windStrength;
    float windRad = glm::radians(m_windAngleDeg);
    ubo.windDir = glm::vec2(std::cos(windRad), std::sin(windRad));

    std::memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Application::processInput(float deltaTime) {
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        m_cameraYaw -= 65.0f * deltaTime;
        m_autoOrbitCamera = false;
    }
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        m_cameraYaw += 65.0f * deltaTime;
        m_autoOrbitCamera = false;
    }
    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_UP) == GLFW_PRESS) {
        m_cameraPitch = std::clamp(m_cameraPitch + 45.0f * deltaTime, -5.0f, 80.0f);
    }
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(m_window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        m_cameraPitch = std::clamp(m_cameraPitch - 45.0f * deltaTime, -5.0f, 80.0f);
    }

    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_cameraDistance = std::clamp(m_cameraDistance - 6.0f * deltaTime, 2.0f, 25.0f);
    }
    if (glfwGetKey(m_window, GLFW_KEY_E) == GLFW_PRESS) {
        m_cameraDistance = std::clamp(m_cameraDistance + 6.0f * deltaTime, 2.0f, 25.0f);
    }
    if (glfwGetKey(m_window, GLFW_KEY_R) == GLFW_PRESS) {
        m_cameraTargetZ = std::clamp(m_cameraTargetZ + 3.0f * deltaTime, -0.5f, 5.0f);
    }
    if (glfwGetKey(m_window, GLFW_KEY_F) == GLFW_PRESS) {
        m_cameraTargetZ = std::clamp(m_cameraTargetZ - 3.0f * deltaTime, -0.5f, 5.0f);
    }

    if (glfwGetKey(m_window, GLFW_KEY_Z) == GLFW_PRESS) {
        m_windAngleDeg = std::fmod(m_windAngleDeg - 60.0f * deltaTime + 360.0f, 360.0f);
    }
    if (glfwGetKey(m_window, GLFW_KEY_X) == GLFW_PRESS) {
        m_windAngleDeg = std::fmod(m_windAngleDeg + 60.0f * deltaTime, 360.0f);
    }
    if (glfwGetKey(m_window, GLFW_KEY_C) == GLFW_PRESS) {
        m_windStrength = std::clamp(m_windStrength - 1.0f * deltaTime, 0.0f, 3.0f);
    }
    if (glfwGetKey(m_window, GLFW_KEY_V) == GLFW_PRESS) {
        m_windStrength = std::clamp(m_windStrength + 1.0f * deltaTime, 0.0f, 3.0f);
    }

    bool nDown = glfwGetKey(m_window, GLFW_KEY_N) == GLFW_PRESS;
    bool tDown = glfwGetKey(m_window, GLFW_KEY_T) == GLFW_PRESS;
    bool gDown = glfwGetKey(m_window, GLFW_KEY_G) == GLFW_PRESS;
    bool lBracket = glfwGetKey(m_window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
    bool rBracket = glfwGetKey(m_window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
    bool minusKey = glfwGetKey(m_window, GLFW_KEY_MINUS) == GLFW_PRESS;
    bool equalKey = glfwGetKey(m_window, GLFW_KEY_EQUAL) == GLFW_PRESS;
    bool oDown = glfwGetKey(m_window, GLFW_KEY_O) == GLFW_PRESS;
    bool kDown = glfwGetKey(m_window, GLFW_KEY_K) == GLFW_PRESS;
    bool lDown = glfwGetKey(m_window, GLFW_KEY_L) == GLFW_PRESS;
    bool pDown = glfwGetKey(m_window, GLFW_KEY_P) == GLFW_PRESS;

    if (nDown || tDown || gDown || lBracket || rBracket || minusKey || equalKey || oDown || kDown || lDown || pDown) {
        if (!m_actionKeyLatch) {
            m_actionKeyLatch = true;
            if (nDown) {
                m_treeParams.seed += 1;
                rebuildSceneBuffers();
            }
            else if (tDown) {
                int nextSpecies = (static_cast<int>(m_treeParams.species) + 1) % static_cast<int>(TreeSpecies::Count);
                m_treeParams = getSpeciesPreset(static_cast<TreeSpecies>(nextSpecies), m_treeParams.seed);
                std::cout << "Switched species to: " << m_treeParams.speciesName << '\n';
                rebuildSceneBuffers();
            }
            else if (gDown) {
                m_forestGroveMode = !m_forestGroveMode;
                m_cameraDistance = m_forestGroveMode ? 14.0f : 8.5f;
                rebuildSceneBuffers();
            }
            else if (lBracket) {
                m_treeParams.branchAngle = std::clamp(m_treeParams.branchAngle - 5.0f, 15.0f, 65.0f);
                rebuildSceneBuffers();
            }
            else if (rBracket) {
                m_treeParams.branchAngle = std::clamp(m_treeParams.branchAngle + 5.0f, 15.0f, 65.0f);
                rebuildSceneBuffers();
            }
            else if (minusKey) {
                m_treeParams.leavesPerTwig = std::clamp(m_treeParams.leavesPerTwig - 4, 0, 50);
                rebuildSceneBuffers();
            }
            else if (equalKey) {
                m_treeParams.leavesPerTwig = std::clamp(m_treeParams.leavesPerTwig + 4, 0, 50);
                rebuildSceneBuffers();
            }
            else if (oDown) {
                m_autoOrbitCamera = !m_autoOrbitCamera;
            }
            else if (kDown) {
                if (TreeSerializer::savePresetToFile("saved_tree.tree", m_treeParams)) {
                    std::cout << "[Saved] Preset written to saved_tree.tree\n";
                }
            }
            else if (lDown) {
                TreeParameters loaded{};
                if (TreeSerializer::loadPresetFromFile("saved_tree.tree", loaded)) {
                    m_treeParams = loaded;
                    std::cout << "[Loaded] Preset read from saved_tree.tree (" << m_treeParams.speciesName << ")\n";
                    rebuildSceneBuffers();
                }
                else {
                    std::cout << "[Load Failed] Could not read saved_tree.tree\n";
                }
            }
            else if (pDown) {
                TreeModel exportModel(m_treeParams);
                if (TreeSerializer::exportTreeToObj("exported_tree.obj", exportModel, m_treeParams.radialSegments)) {
                    std::cout << "[Exported] 3D mesh written to exported_tree.obj (ready for Unreal/Unity/Godot/Blender)\n";
                }
            }
        }
    }
    else {
        m_actionKeyLatch = false;
    }
}

void Application::drawFrame() {
    // Wait for the previous frame using this slot to finish on the GPU
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    // Acquire the next available image from the swapchain
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(
        m_device,
        m_swapChain,
        UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    // Only reset the fence once command buffer finished executing on the GPU
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);

    // Update the 3D camera and model matrices for this frame slot
    updateUniformBuffer(m_currentFrame);

    // Reset and record the command buffer for this image
    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

    // Submit the recorded command buffer to the graphics queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

    VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[imageIndex] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // Present the rendered image back to the swapchain
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { m_swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight;
}

void Application::cleanupSwapChain() {
    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }

    if (m_depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_depthImage, nullptr);
        m_depthImage = VK_NULL_HANDLE;
    }

    if (m_depthImageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_depthImageMemory, nullptr);
        m_depthImageMemory = VK_NULL_HANDLE;
    }

    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    m_swapChainFramebuffers.clear();

    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    m_swapChainImageViews.clear();

    if (m_swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
        m_swapChain = VK_NULL_HANDLE;
    }
}

void Application::recreateSwapChain() {
    // Pause if the window is minimised (width or height == 0)
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
}

void Application::framebufferResizeCallback(GLFWwindow* window, int /*width*/, int /*height*/) {
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->m_framebufferResized = true;
}

void Application::mainLoop() {

    std::cout << "\n=== Interactive Tree Viewer Controls ===\n"
        << "  [W/A/S/D] or [Arrows] : Orbit Camera  | [Q/E] Zoom | [R/F] Pan Height | [O] Toggle Auto-Orbit\n"
        << "  [T]                   : Cycle Species (Oak -> Sakura -> Silver Birch -> Weeping Willow -> Scots Pine)\n"
        << "  [1/2/3/4]             : Seasons       | [Space] Toggle Auto-Season Cycle\n"
        << "  [Z / X]               : Wind Compass  | [C / V] Decrease/Increase Wind Strength\n"
        << "  [N]                   : New Tree Seed | [G] Toggle 5-Species Botanical Grove\n"
        << "  [ / ]                 : Branch Angle  | [- / =] Decrease/Increase Leaf Density\n"
        << "  [K] Save Preset       | [L] Load Preset | [P] Export 3D .obj Mesh for Game Engines\n\n";

    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }

        // Seasonal preset hotkeys (1 = Spring, 2 = Summer, 3 = Autumn, 4 = Winter)
        if (glfwGetKey(m_window, GLFW_KEY_1) == GLFW_PRESS) {
            m_season = 0.25f;
            m_autoCycleSeasons = false;
        }
        if (glfwGetKey(m_window, GLFW_KEY_2) == GLFW_PRESS) {
            m_season = 1.0f;
            m_autoCycleSeasons = false;
        }
        if (glfwGetKey(m_window, GLFW_KEY_3) == GLFW_PRESS) {
            m_season = 1.75f; // Mid-autumn so bough-by-bough green/yellow/orange contrast is clearly visible
            m_autoCycleSeasons = false;
        }
        if (glfwGetKey(m_window, GLFW_KEY_4) == GLFW_PRESS) {
            m_season = 3.2f;
            m_autoCycleSeasons = false;
        }

        // Toggle automatic seasonal progression on Spacebar press
        if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (!m_spaceKeyPressed) {
                m_autoCycleSeasons = !m_autoCycleSeasons;
                m_spaceKeyPressed = true;
            }
        }
        else {
            m_spaceKeyPressed = false;
        }
        drawFrame();
    }
    vkDeviceWaitIdle(m_device);
}

void Application::cleanup() {
    cleanupSwapChain();

    for (size_t i = 0; i < m_uniformBuffers.size(); ++i) {
        if (m_uniformBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, m_uniformBuffers[i], nullptr);
        }
        if (m_uniformBuffersMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_uniformBuffersMemory[i], nullptr);
        }
    }
    m_uniformBuffers.clear();
    m_uniformBuffersMemory.clear();
    m_uniformBuffersMapped.clear();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }

    auto destroyBufferPair = [this](VkBuffer& buf, VkDeviceMemory& mem) {
        if (buf != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, buf, nullptr);
            buf = VK_NULL_HANDLE;
        }
        if (mem != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, mem, nullptr);
            mem = VK_NULL_HANDLE;
        }
        };

    destroyBufferPair(m_leafInstanceBuffer, m_leafInstanceBufferMemory);
    destroyBufferPair(m_leafIndexBuffer, m_leafIndexBufferMemory);
    destroyBufferPair(m_leafVertexBuffer, m_leafVertexBufferMemory);
    destroyBufferPair(m_trunkInstanceBuffer, m_trunkInstanceBufferMemory);

    destroySceneBuffers();

    if (m_graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        m_graphicsPipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    for (VkSemaphore semaphore : m_renderFinishedSemaphores) {
        vkDestroySemaphore(m_device, semaphore, nullptr);
    }
    m_renderFinishedSemaphores.clear();

    for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
        if (i < m_imageAvailableSemaphores.size()) {
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        }
        if (i < m_inFlightFences.size()) {
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }
    }
    m_imageAvailableSemaphores.clear();
    m_inFlightFences.clear();

    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();
    std::cout << "Application cleaned up.\n";
}