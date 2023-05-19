# Adds the target and creates placeholder headers
file(GLOB_RECURSE ispc_files *.ispc)
ispc_add_prebuild_target(prebuild_curve ispc_files)

# Gather the placeholder objs that will be filled in by the prebuild target
file(GLOB_RECURSE ispc_objs out_ispc/*.obj)

# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h *.ispc)

add_library(curve STATIC ${source_files})

target_link_libraries(curve ${ispc_objs})

# Include this directory to anyone that includes this cmakelists
target_include_directories(curve INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_precompile_headers(curve PRIVATE src/Precompile.h)

# Include directories for this
target_include_directories(curve
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/out_ispc
)
