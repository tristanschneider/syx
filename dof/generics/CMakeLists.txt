# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h)

add_library(generics STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
target_include_directories(generics INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_precompile_headers(generics PRIVATE src/Precompile.h)

# Include directories for this
target_include_directories(generics
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
