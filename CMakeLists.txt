cmake_minimum_required(VERSION 3.18)

project("foray_restir")

# Include Compiler Config (sets c++ 20 and compiler flags)
include("foray_rt_rpf/cmakescripts/compilerconfig.cmake")

# Run Locate SDL2 script
set(SDL2_HINT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/foray_rt_rpf/third_party")
include("foray_rt_rpf/cmakescripts/locatesdl2.cmake")

# Add subdirectories
add_subdirectory("foray_rt_rpf")
add_subdirectory("src")