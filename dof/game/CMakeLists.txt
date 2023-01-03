# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h *.ispc)

# Adds the target and creates placeholder headers
file(GLOB_RECURSE ispc_files *.ispc)
ispc_add_prebuild_target(prebuild_game ispc_files)
# Gather the placeholder objs that will be filled in by the prebuild target
file(GLOB_RECURSE ispc_objs out_ispc/*.obj)

add_library(game STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
target_include_directories(game INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Include directories for this
target_include_directories(game
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/out_ispc)

target_precompile_headers(game PRIVATE Precompile.h)

# Link dependencies
target_link_libraries(game PUBLIC glm)
target_link_libraries(game PUBLIC physics)
target_link_libraries(game PUBLIC table)
target_link_libraries(game PRIVATE ${ispc_objs})
