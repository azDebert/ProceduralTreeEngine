#include "Application.h"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
    try {
        Application app(1280, 720, "Procedural Tree Engine - Vulkan");
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}