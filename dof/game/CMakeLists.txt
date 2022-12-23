# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h)

add_library(game STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
target_include_directories(game INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Include directories for this
target_include_directories(game PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(game PRIVATE Precompile.h)

# Link dependencies
target_link_libraries(game PUBLIC physics)
target_link_libraries(game PUBLIC table)