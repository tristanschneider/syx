set(BUILD_DOC OFF CACHE BOOL "" FORCE)
set(BUILD_SANDBOX OFF CACHE BOOL "" FORCE)
set(SKIP_PERFORMANCE_COMPARISON ON CACHE BOOL "" FORCE)
set(JUST_INSTALL_CEREAL ON CACHE BOOL "" FORCE)

# Warnings in cereal in c++20
add_compile_options(/wd26451 /wd5054 /wd26495)

add_subdirectory(cereal)

# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files src/*.cpp src/*.h include/*.h)

add_library(config STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
target_include_directories(config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_precompile_headers(config PRIVATE src/Precompile.h)

# Include directories for this
target_include_directories(config
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cereal/include
)

target_link_libraries(config PRIVATE cereal)
target_link_libraries(config PUBLIC glm)
