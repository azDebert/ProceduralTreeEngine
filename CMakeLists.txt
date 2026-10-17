cmake_minimum_required(VERSION 3.25)

# Define the project name, version, and programming language (CXX = C++)
project(ProceduralTreeEngine VERSION 0.1.0 LANGUAGES CXX)

# Enforce C++20 strictly across all targets
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create an executable program named "TreeEngine" from our main source file
add_executable(TreeEngine
    src/main.cpp
)