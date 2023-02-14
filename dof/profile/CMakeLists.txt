cmake_minimum_required(VERSION 3.10)

add_library(profile INTERFACE)

if(MSVC)
    # Warnings in microprofile source
    add_compile_options(/wd4456 /wd4189)
endif()


if(PROFILE_ENABLED)
    add_subdirectory(microprofile)
    target_link_libraries(profile INTERFACE microprofile)

    target_include_directories(profile
        INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/microprofile
    )

    target_include_directories(profile INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/enabled)
    file(GLOB_RECURSE source enabled/*.h)
    target_precompile_headers(microprofile PRIVATE Precompile.h)
else()
    target_include_directories(profile INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/disabled)
    file(GLOB_RECURSE source disabled/*.h)
endif()

target_sources(profile PUBLIC ${source})