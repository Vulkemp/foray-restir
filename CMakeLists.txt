cmake_minimum_required(VERSION 3.18)

project("foray_restir")

# Include Compiler Config (sets c++ 20 and compiler flags)
include("foray/cmakescripts/compilerconfig.cmake")

# Run Locate SDL2 script
set(SDL2_HINT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/foray/third_party")
include("foray/cmakescripts/locatesdl2.cmake")

# Add subdirectories
add_subdirectory("foray")
add_subdirectory("src")