# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h)

add_library(poa STATIC ${source_files})

target_link_libraries(poa ${CMAKE_CURRENT_SOURCE_DIR}/test.obj)

# Include this directory to anyone that includes this cmakelists
target_include_directories(poa INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Include directories for this
target_include_directories(poa PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# None for now
# target_precompile_headers(engine_shared PRIVATE Precompile.h)

file(GLOB_RECURSE ispc_files *.ispc)

message("ispc files ${ispc_files}")

ispc_add_prebuild_target(prebuild_poa ispc_files)
add_dependencies(poa prebuild_poa)
