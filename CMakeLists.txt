cmake_minimum_required(VERSION 3.25)

# Define the project name, version, and programming language (CXX = C++)
project(ProceduralTreeEngine VERSION 0.1.0 LANGUAGES CXX)

# Enforce C++20 strictly across all targets
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find LunarG Vulkan SDK
find_package(Vulkan REQUIRED)

# Automatically download and build GLFW for window and input
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)

# Disable building GLFW tests, examples and documentation to keep the build fast
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# Define an executable program from its source/header files
add_executable(TreeEngine
    src/main.cpp
    src/Application.h
    src/Application.cpp
)

# Link GLFW and Vulkan to the executable
target_link_libraries(TreeEngine PRIVATE
    glfw
    Vulkan::Vulkan
)