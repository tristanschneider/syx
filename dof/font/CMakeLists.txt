file(GLOB_RECURSE source_files
    fontstash/src/*.h
)

add_library(font INTERFACE)

# Include this directory to anyone that includes this cmakelists
target_include_directories(font INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/fontstash/src
)

target_sources(font INTERFACE ${source_files})