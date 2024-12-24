cmake_minimum_required(VERSION 3.26)

# Project name
project(dof)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build_output)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED)

option(TEST_ENABLED "Include test projects" ON)
option(PROFILE_ENABLED "Include profiler" ON)
option(IMGUI_ENABLED "Include imgui" ON)
option(PERFORMANCE_ENABLED "Build performance test" ON)

add_compile_options(/wd4309 /wd4324 /wd4389 /wd4127 /wd4267 /wd4146 /wd4201 /wd4464 /wd4514 /wd4701 /wd4820 /wd4365)

if(MSVC)
    add_compile_definitions(
        _HAS_EXCEPTIONS=0
        _CRT_SECURE_NO_WARNINGS
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    )
    add_compile_options(
        /W4
        /WX
        # Disable RTTI
        /GR-
        # Disable most of exception handling
        /EHsc
        # Structure was padded due to alignment specifier
        /wd4324
        # Truncation of constant value
        /wd4309
        # Always stl non-debug for speed
        /MT
        # Multiprocessor compilation
        /MP
    )
    add_link_options(
        /WX
    )
endif()

# Intended to be called by the executable that imports this directly or indirectly
function(copy_data_to_target target)
    # Build file list
    set(data_dir "${CMAKE_SOURCE_DIR}/data/")

    # Copy list to target
    add_custom_command(TARGET ${target} COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${data_dir}"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/data"
    )
endfunction()

# No dependencies (self contained)
add_subdirectory(generics)
add_subdirectory(scheduler)
add_subdirectory(profile)
add_subdirectory(glm)
add_subdirectory(stb)
add_subdirectory(sokol)
add_subdirectory(ispc)

# With dependencies
add_subdirectory(table)
add_subdirectory(loader)
add_subdirectory(curve)
add_subdirectory(physics)
add_subdirectory(config)
add_subdirectory(game)
add_subdirectory(renderer)

if(TEST_ENABLED)
    add_subdirectory(test)
endif()

if(IMGUI_ENABLED)
    add_compile_definitions(IMGUI_ENABLED=1)
    add_subdirectory(imgui)
endif()

if(WIN32)
    add_subdirectory(win32)
    if(MSVC)
        set(VS_STARTUP_PROJECT win32)
    endif()
endif()

if(PERFORMANCE_ENABLED)
    add_subdirectory(performance)
endif()