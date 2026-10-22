#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "TreeModel.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// Represents a single vertex transferred from the CPU to the GPU
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec3 normal;
    float flexibility = 0.0f; // >0 = branch wind flexibility, 0 = ground, -1 = wind compass arrow

    // Describes Binding 0 (per-vertex) and Binding 1 (per-instance LeafInstance)
    static std::array<VkVertexInputBindingDescription, 2> getBindingDescriptions() {
        std::array<VkVertexInputBindingDescription, 2> bindings{};

        // --- Binding 0: advances once per vertex ---
        bindings[0].binding = 0;
        bindings[0].stride = sizeof(Vertex);
        bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // --- Binding 1: advances once per drawn instance ---
        bindings[1].binding = 1;
        bindings[1].stride = sizeof(LeafInstance);
        bindings[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        return bindings;
    }

    // Maps Vertex (locations 0, 1, 2, 10) and LeafInstance (locations 3..9, 11) to the vertex shader
    static std::array<VkVertexInputAttributeDescription, 12> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 12> attributes{};

        // --- Binding 0: Per-Vertex Attributes ---
        attributes[0] = { 0,  0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) };
        attributes[1] = { 1,  0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) };
        attributes[2] = { 2,  0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) };
        attributes[10] = { 10, 0, VK_FORMAT_R32_SFLOAT,       offsetof(Vertex, flexibility) };

        // --- Binding 1: Per-Instance Attributes (LeafInstance) ---
        attributes[3] = { 3,  1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeafInstance, position) };
        attributes[4] = { 4,  1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeafInstance, direction) };
        attributes[5] = { 5,  1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeafInstance, normal) };
        attributes[6] = { 6,  1, VK_FORMAT_R32_SFLOAT,       offsetof(LeafInstance, scale) };
        attributes[7] = { 7,  1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeafInstance, color) };
        attributes[8] = { 8,  1, VK_FORMAT_R32_SFLOAT,       offsetof(LeafInstance, phase) };
        attributes[9] = { 9,  1, VK_FORMAT_R32_SINT,         offsetof(LeafInstance, speciesType) };
        attributes[11] = { 11, 1, VK_FORMAT_R32_SFLOAT,       offsetof(LeafInstance, flexibility) };

        return attributes;
    }
};

// 3D transformation matrices and environmental uniforms sent to the vertex shader
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(4) float season;
    alignas(4) float time;
    alignas(4) float windStrength;
    alignas(8) glm::vec2 windDir;
};

// Sub-mesh index range and instance batch range for each species' 3D foliage mesh
struct FoliageDrawBatch {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    int32_t vertexOffset = 0;
    uint32_t firstInstance = 0;
    uint32_t instanceCount = 0;
};

// Holds the queue family indices supported by GPU
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Application {
public:
    Application(uint32_t width, uint32_t height, const std::string& title);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void initWindow();
    void initVulkan();
    void createInstance();
    bool checkValidationLayerSupport() const;

    void createSurface();
    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();

    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    static std::vector<char> readFile(const std::string& filename);

    void createFramebuffers();
    void createCommandPool();
    void createDepthResources();
    VkFormat findSupportedFormat(
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    ) const;
    VkFormat findDepthFormat() const;
    void createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory
    ) const;
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;
    void createSceneBuffers();
    void destroySceneBuffers();
    void rebuildSceneBuffers();
    void processInput(float deltaTime);
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateUniformBuffer(uint32_t currentImage);
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    ) const;
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) const;
    void createSyncObjects();
    void drawFrame();
    void recreateSwapChain();
    void cleanupSwapChain();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void mainLoop();
    void cleanup();

    uint32_t m_width;
    uint32_t m_height;
    std::string m_title;

    GLFWwindow* m_window = nullptr;
    VkInstance m_instance = VK_NULL_HANDLE;

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;

    VkQueue m_presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapChainImages;
    VkFormat m_swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapChainExtent{};
    std::vector<VkImageView> m_swapChainImageViews;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_depthImageMemory = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> m_swapChainFramebuffers;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
    uint32_t m_indexCount = 0;

    VkBuffer m_trunkInstanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_trunkInstanceBufferMemory = VK_NULL_HANDLE;

    VkBuffer m_leafVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_leafVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_leafIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_leafIndexBufferMemory = VK_NULL_HANDLE;
    VkBuffer m_leafInstanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_leafInstanceBufferMemory = VK_NULL_HANDLE;
    uint32_t m_leafInstanceCount = 0;
    std::vector<FoliageDrawBatch> m_foliageBatches;

    std::vector<VkBuffer> m_uniformBuffers;
    std::vector<VkDeviceMemory> m_uniformBuffersMemory;
    std::vector<void*> m_uniformBuffersMapped;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;

    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
    bool m_framebufferResized = false;

    float m_season = 0.0f;          // 0.0 = Spring, 1.0 = Summer, 2.0 = Autumn, 3.0 = Winter
    bool m_autoCycleSeasons = true; // Automatically transition smoothly through seasons
    bool m_spaceKeyPressed = false;

    // Interactive camera orbit, pan, and zoom state
    float m_cameraYaw = 0.0f;
    float m_cameraPitch = 15.0f;
    float m_cameraDistance = 8.5f;
    float m_cameraTargetZ = 1.2f;
    bool m_autoOrbitCamera = true;

    // Interactive tree, wind, and grove state
    TreeParameters m_treeParams = getSpeciesPreset(TreeSpecies::EnglishOak, 42);
    float m_windAngleDeg = 45.0f;   // Wind compass direction in degrees (0=N, 90=E, 180=S, 270=W)
    bool m_forestGroveMode = false; // Toggle between 1 tree and a 5-tree grove
    float m_windStrength = 1.0f;
    bool m_actionKeyLatch = false;

    // Performance profiling state
    uint32_t m_frameCount = 0;
    float m_fpsTimer = 0.0f;
};