cmake_minimum_required(VERSION 3.0)

set(ENKITS_BUILD_C_INTERFACE OFF)
set(ENKITS_BUILD_EXAMPLES OFF)
set(ENKITS_TASK_PRIORITIES_NUM 1)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

if(MSVC)
    add_compile_options(/wd4244)
endif()

add_subdirectory(enkiTS)

add_library(scheduler INTERFACE)

target_link_libraries(scheduler INTERFACE enkiTS)

target_include_directories(scheduler INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB_RECURSE source *.h)
target_sources(scheduler PUBLIC ${source})
