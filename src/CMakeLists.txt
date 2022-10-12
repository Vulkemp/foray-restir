cmake_minimum_required(VERSION 3.18)

project("foray_restir_app")

MESSAGE("--- << CMAKE of ${PROJECT_NAME} >> --- ")
MESSAGE(STATUS "CURRENT SOURCE DIR \"${CMAKE_CURRENT_SOURCE_DIR}\"")#

# Enable strict mode for own code
SET(CMAKE_CXX_FLAGS ${STRICT_FLAGS})

# collect sources
file(GLOB_RECURSE src "*.cpp")
if (DISABLE_RT_EXTENSIONS)
    add_compile_definitions(DISABLE_RT_EXTENSIONS)
endif()
if (ENABLE_GBUFFER_BENCH)
    add_compile_definitions(ENABLE_GBUFFER_BENCH)
endif()
add_executable(${PROJECT_NAME} ${src})

# find vulkan
find_package(Vulkan REQUIRED)

target_link_libraries(
	${PROJECT_NAME}
	PUBLIC rtrpf
)

if (WIN32)
	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC ${SDL2_LIBRARIES}
	)
endif()

target_include_directories(
	${PROJECT_NAME}
	PUBLIC "../foray_rt_rpf/src"
	PUBLIC "../foray_rt_rpf/third_party"
	PUBLIC ${Vulkan_INCLUDE_DIRS}
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/configurepath.hpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/configurepath.cmakegenerated.hpp")