file(GLOB source_imgui imgui/*.cpp imgui/*.h)
file(GLOB_RECURSE source_files src/*.cpp src/*.h include/*.h)

list(FILTER source_imgui EXCLUDE REGEX "imgui/imgui_demo.*")

add_library(imgui STATIC ${source_files} ${source_imgui})

# Include this directory to anyone that includes this cmakelists
target_include_directories(imgui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Include directories for this
target_include_directories(imgui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

target_precompile_headers(game PRIVATE src/Precompile.h)

# Link dependencies
target_link_libraries(imgui PRIVATE game ogl glew glm)