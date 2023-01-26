function (foray_example)

    # name project after the folder its located in
    get_filename_component(proj_name ${CMAKE_CURRENT_LIST_DIR} NAME)

    project(${proj_name})

    MESSAGE("--- << CMAKE of ${PROJECT_NAME} >> --- ")
    MESSAGE(STATUS "CURRENT SOURCE DIR \"${CMAKE_CURRENT_SOURCE_DIR}\"")

    # collect sources
    file(GLOB_RECURSE src "*.cpp")
    
    # Make sure there are source files, add_executable would otherwise fail
    if (NOT src)
        message(WARNING "Project \"${PROJECT_NAME}\" does not contain any source files")
        return()
    endif ()

    # Declare executable
    add_executable(${PROJECT_NAME} ${src})
    
    # Set strict mode for project only
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS ${STRICT_FLAGS})

    # Set directories via compile macros
    target_compile_options(${PROJECT_NAME} PUBLIC "-DCWD_OVERRIDE=\"${CMAKE_CURRENT_LIST_DIR}\"")
    target_compile_options(${PROJECT_NAME} PUBLIC "-DDATA_DIR=\"${CMAKE_SOURCE_DIR}/data\"")
    target_compile_options(${PROJECT_NAME} PUBLIC "-DFORAY_SHADER_DIR=\"$CACHE{FORAY_SHADER_DIR}\"")

    # Link foray lib
    target_link_libraries(
    	${PROJECT_NAME}
    	PUBLIC foray
    )

    # Windows requires SDL2 libs linked specifically
    if (WIN32)
    	target_link_libraries(
    		${PROJECT_NAME}
    		PUBLIC ${SDL2_LIBRARIES}
    	)
    endif()


    # Configure include directories
    target_include_directories(
    	${PROJECT_NAME}
    	PUBLIC "${CMAKE_SOURCE_DIR}/foray/src"
    	PUBLIC "${CMAKE_SOURCE_DIR}/foray/third_party"
    	PUBLIC ${Vulkan_INCLUDE_DIR}
    )
endfunction()
