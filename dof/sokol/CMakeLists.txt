# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files
    sokol/*.h
    sokol/util/*.h
)

add_library(sokol INTERFACE)

# Include this directory to anyone that includes this cmakelists
target_include_directories(sokol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/sokol)

target_sources(sokol INTERFACE ${source_files})
