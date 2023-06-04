file(GLOB_RECURSE source_files *.cpp *.h)

# Declare this executable
add_executable(win32 WIN32 ${source_files})

# Link with dependencies
target_link_libraries(win32 PUBLIC game)
target_link_libraries(win32 PUBLIC ogl)
if(IMGUI_ENABLED)
    target_link_libraries(win32 PUBLIC imgui)
endif()

target_link_libraries(win32 PRIVATE glew)
target_link_libraries(win32 PRIVATE opengl32.lib)

target_include_directories(win32 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_compile_definitions(NOMINMAX)

copy_glew_to_target(win32)
copy_data_to_target(win32)

target_precompile_headers(win32 PRIVATE Precompile.h)

# When debugging, point the default project at src data instead of output data so edits can be made in-place if desired
set(data_dir "${CMAKE_SOURCE_DIR}/data/")
set_target_properties(win32 PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "${data_dir}")
