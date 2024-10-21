file(GLOB_RECURSE source_files src/*.cpp src/*.h include/*.h)

add_library(loader STATIC ${source_files})

target_include_directories(loader INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_precompile_headers(loader PRIVATE src/Precompile.h)

target_include_directories(loader
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(ASSIMP_NO_EXPORT ON)
set(ASSIMP_INSTALL OFF)
set(ASSIMP_BUILD_ZLIB ON)
set(ASSIMP_BUILD_TESTS OFF)
set(USE_STATIC_CRT ON)
add_compile_options(/GR)

add_subdirectory(assimp)

target_link_libraries(loader
    PRIVATE assimp
    PRIVATE table
    PRIVATE glm
)