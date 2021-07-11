# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp)

add_library(physics STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
target_include_directories(physics INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
