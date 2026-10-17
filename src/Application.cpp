#include "Application.h"

#include <iostream>
#include <stdexcept>

Application::Application(uint32_t width, uint32_t height, const std::string& title)
    : m_width(width), m_height(height), m_title(title) {
    initWindow();
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

void Application::mainLoop() {
    // Keep looping until the user clicks the 'X' button or presses Escape
    while (!glfwWindowShouldClose(m_window)) {
        // Check for window events (keyboard presses, mouse movement, window close)
        glfwPollEvents();

        // Allow closing the window by pressing the Escape key
        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
    }
}

void Application::cleanup() {
    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    std::cout << "Application resources cleaned up cleanly.\n";
}