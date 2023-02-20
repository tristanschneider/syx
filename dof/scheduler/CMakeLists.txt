cmake_minimum_required(VERSION 3.0)

set(ENKITS_BUILD_C_INTERFACE OFF)
set(ENKITS_BUILD_EXAMPLES OFF)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

if(MSVC)
    add_compile_options(/wd4244)
endif()

add_subdirectory(enkiTS)

add_library(scheduler ALIAS enkiTS)