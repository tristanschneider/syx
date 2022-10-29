cmake_minimum_required(VERSION 3.10)

# Project name
project(syx)

option(TEST_ENABLED "Include test projects" ON)
option(PERFTEST_ENABLED "Include performance tests" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/build_output)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_SOURCE_DIR}/vcpkg/packages")
set(VCPKG_TARGET_TRIPLE "x64-windows")
set(VCPKG_BASE "${CMAKE_SOURCE_DIR}/vcpkg")
set(VCPKG_ARCH "x64-windows")

#TODO: less platform specific
function(vcpkg_install package)
    set(vcpkg_base "${CMAKE_SOURCE_DIR}/vcpkg")
    set(vcpkg_path "${vcpkg_base}/vcpkg.exe")

    if(NOT EXISTS "${vcpkg_path}")
        execute_process(COMMAND git clone https://github.com/microsoft/vcpkg.git)
        execute_process(COMMAND "${vcpkg_base}/bootstrap-vcpkg.bat")
    endif()
    execute_process(COMMAND "${vcpkg_path}" --triplet ${VCPKG_ARCH} install "${package}")
endfunction()

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

add_subdirectory(ispc)
add_subdirectory(poa)
add_subdirectory(ecx)
add_subdirectory(syx)
add_subdirectory(src-deps)
add_subdirectory(lua)
add_subdirectory(Engine.Shared)

if(TEST_ENABLED)
    add_subdirectory(test)
endif()

if(PERFTEST_ENABLED)
    add_subdirectory(perftest)
endif()

if(WIN32)
    # Might not be needed
    add_subdirectory(Win32.Shared)
    if(MSVC)
        set(VS_STARTUP_PROJECT win32)
    endif()
endif()
