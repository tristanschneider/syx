# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h)

add_library(ogl STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
target_include_directories(ogl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Include directories for this
target_include_directories(ogl PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(ogl PRIVATE Precompile.h)

# Link dependencies
target_link_libraries(ogl PRIVATE table)
target_link_libraries(ogl PRIVATE game)
target_link_libraries(ogl PRIVATE glew)