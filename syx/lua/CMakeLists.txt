# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.c)

file(GLOB_RECURSE exclude_files *luac.c)
list(REMOVE_ITEM source_files ${exclude_files})

add_library(lua STATIC ${source_files})

target_include_directories(lua INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
