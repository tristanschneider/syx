cmake_minimum_required(VERSION 3.10)

# Project name
project(syx)

option(TEST_ENABLED "Include test projects" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build_output)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED)

# Always disable because it's mega slow
add_definitions(-DNDEBUG)

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
    )
    add_link_options(
        /WX
    )
endif()

add_subdirectory(ecx)
add_subdirectory(syx)
add_subdirectory(src-deps)
add_subdirectory(lua)
add_subdirectory(Engine.Shared)

if(TEST_ENABLED)
    add_subdirectory(test)
endif()

if(WIN32)
    # Might not be needed
    add_subdirectory(Win32.Shared)
    if(MSVC)
        set(VS_STARTUP_PROJECT win32)
    endif()
endif()