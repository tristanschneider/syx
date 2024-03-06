file(GLOB_RECURSE source_files *.cpp *.h)

# Declare this executable
add_executable(performance ${source_files})

# Link with dependencies
target_link_libraries(performance PUBLIC game)

target_include_directories(performance PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_compile_definitions(NOMINMAX)

target_precompile_headers(performance PRIVATE Precompile.h)
