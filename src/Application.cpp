#include "Application.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    // Validation layer used for catching Vulkan bugs
    const std::vector<const char*> kValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
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

    std::cout << "GLFW Window (" << m_width << "x" << m_height << ") initialised successfully.\n";
}

void Application::initVulkan() {
	createInstance();
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

void Application::mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
    }
}

void Application::cleanup() {
    // Destroy Vulkan objects in the reverse order of creation
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