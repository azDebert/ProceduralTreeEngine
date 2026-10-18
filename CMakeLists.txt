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

# Automatically download GLM for 3D vectors and matrices
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)

FetchContent_MakeAvailable(glfw glm)

# Define an executable program from its source/header files
add_executable(TreeEngine
    src/main.cpp
    src/Application.h
    src/Application.cpp
    src/TreeModel.h
)

# Link GLFW, GLM, and Vulkan to the executable
target_link_libraries(TreeEngine PRIVATE
    glfw
    glm::glm
    Vulkan::Vulkan
)

# Automatically compile GLSL shaders into SPIR-V (.spv)
find_program(GLSLC_EXECUTABLE NAMES glslc HINTS Vulkan::glslc REQUIRED)

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_FILES
    "${SHADER_SOURCE_DIR}/triangle.vert"
    "${SHADER_SOURCE_DIR}/triangle.frag"
)

foreach(SHADER ${SHADER_FILES})
    get_filename_component(FILE_NAME ${SHADER} NAME)
    set(SPV_FILE "${CMAKE_CURRENT_BINARY_DIR}/shaders/${FILE_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TreeEngine>/shaders"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND ${GLSLC_EXECUTABLE} ${SHADER} -o ${SPV_FILE}
        COMMAND ${CMAKE_COMMAND} -E copy ${SPV_FILE} "$<TARGET_FILE_DIR:TreeEngine>/shaders/${FILE_NAME}.spv"
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${FILE_NAME} to SPIR-V"
    )
    list(APPEND SPV_SHADERS ${SPV_FILE})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPV_SHADERS})
add_dependencies(TreeEngine Shaders)

# Enable CTest and build the standalone unit test executable for the procedural tree core
enable_testing()

add_executable(TreeTests
    tests/TreeModelTests.cpp
    src/TreeModel.h
)

target_link_libraries(TreeTests PRIVATE
    glm::glm
)

add_test(NAME TreeModelUnitTests COMMAND TreeTests)