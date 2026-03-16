# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.c *.h)
set(library_name c_std)

add_library(${library_name} STATIC ${source_files})

target_include_directories(${library_name}
PUBLIC
src
)
