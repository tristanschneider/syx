cmake_minimum_required(VERSION 3.10)

# Project name
project(dof)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build_output)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED)

option(TEST_ENABLED "Include test projects" ON)

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

add_subdirectory(glm)
add_subdirectory(stb)
add_subdirectory(table)
add_subdirectory(ispc)
add_subdirectory(physics)
add_subdirectory(game)
add_subdirectory(glew)
add_subdirectory(ogl)

if(TEST_ENABLED)
    add_subdirectory(test)
endif()


if(WIN32)
    add_subdirectory(win32)
    if(MSVC)
        set(VS_STARTUP_PROJECT win32)
    endif()
endif()
