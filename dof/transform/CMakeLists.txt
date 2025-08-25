# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h)

add_library(transform STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
target_include_directories(transform INTERFACE include)

target_precompile_headers(transform PRIVATE src/Precompile.h)

target_link_libraries(transform
    PUBLIC
    table
    glm
    math
    loader
)

# Include directories for this
target_include_directories(transform
    PRIVATE include
    PRIVATE src
)
