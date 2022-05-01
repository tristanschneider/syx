# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h)

add_library(engine_shared STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
target_include_directories(engine_shared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Include directories for this
target_include_directories(engine_shared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link dependencies
target_link_libraries(engine_shared PUBLIC ecx)
target_link_libraries(engine_shared PUBLIC physics)
target_link_libraries(engine_shared PUBLIC imgui)
# Glew should ultimately be extracted to a seperate graphics library
target_link_libraries(engine_shared PRIVATE glew)
target_link_libraries(engine_shared PUBLIC lua)

# Asset Importer
vcpkg_install("assimp")
find_package(assimp CONFIG REQUIRED)
target_link_libraries(engine_shared PRIVATE assimp::assimp)

target_precompile_headers(engine_shared PRIVATE Precompile.h)

# Intended to be called by the executable that imports this directly or indirectly
function(copy_data_to_target target)
    # Build file list
    set(data_dir "${CMAKE_SOURCE_DIR}/data/")

    # Copy list to target
    add_custom_command(TARGET ${target} COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${data_dir}"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/data"
    )
endfunction()