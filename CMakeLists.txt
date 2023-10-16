cmake_minimum_required(VERSION 3.18)

project("foray_restir")

# Include Compiler Config (sets c++ 20 and compiler flags)
include("foray/cmakescripts/compilerconfig.cmake")

# Add cmake script dir to cmake's ""Path""
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Add subdirectories
add_subdirectory("foray")

include(foray_example)

add_subdirectory("src")
add_subdirectory("research_apps/complex-raytracer")