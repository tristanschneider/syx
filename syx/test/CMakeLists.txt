# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h)

# Test runner expects a dll (SHARED)
add_library(test SHARED ${source_files})

target_link_libraries(test PUBLIC engine_shared)

# Glew dependency from shared is leaking in here
# TODO: remove glew dependency
target_link_libraries(test PRIVATE glew)
target_link_libraries(test PRIVATE opengl32.lib)

target_include_directories(test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(test PRIVATE Precompile.h)
