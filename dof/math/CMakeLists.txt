# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h)
set(library_name math)

add_library(${library_name} STATIC ${source_files})

target_include_directories(${library_name}
PUBLIC
include
PRIVATE
src
)

target_precompile_headers(${library_name} PRIVATE src/Precompile.h)

target_link_libraries(${library_name}
    glm
)
