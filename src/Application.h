#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <string>

class Application {
public:
    Application(uint32_t width, uint32_t height, const std::string& title);
    ~Application();

    // Prevent accidental copying of the Application and its underlying window handle
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    void initWindow();
    void mainLoop();
    void cleanup();

    uint32_t m_width;
    uint32_t m_height;
    std::string m_title;

    // Pointer to the GLFW window object created on the heap
    GLFWwindow* m_window = nullptr;
};