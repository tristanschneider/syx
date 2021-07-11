# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h)

file(GLOB_RECURSE exclude_files *imgui_demo.cpp)
list(REMOVE_ITEM source_files ${exclude_files})

add_library(imgui STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
# Might want to move this down a directory so this doesn't include all of src-deps
get_filename_component(imgui_dir ${CMAKE_CURRENT_SOURCE_DIR}../../ ABSOLUTE)
target_include_directories(imgui INTERFACE ${imgui_dir})

# Include directories for this
target_include_directories(imgui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
