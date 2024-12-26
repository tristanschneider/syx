# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h *.glsl)

add_library(renderer STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
target_include_directories(renderer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Include directories for this
target_include_directories(renderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(renderer PRIVATE Precompile.h)

# Link dependencies
target_link_libraries(renderer PRIVATE glm)
target_link_libraries(renderer PRIVATE table)
target_link_libraries(renderer PRIVATE game)
# TODO: remove this by loading from loader project
target_link_libraries(renderer PRIVATE stb)
target_link_libraries(renderer PRIVATE sokol font)

file(GLOB_RECURSE shaders "shaders/*.glsl")
compile_shaders(renderer shaders)