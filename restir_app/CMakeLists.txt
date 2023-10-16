cmake_minimum_required(VERSION 3.18)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/configurepath.hpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/configurepath.cmakegenerated.hpp")

foray_example()
