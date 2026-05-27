file(GLOB_RECURSE src src/*.c src/*.h src/*.glsl)

add_library(sandbox STATIC ${src})

target_include_directories(sandbox PUBLIC src)

target_link_libraries(sandbox
PRIVATE
    sokol
    c_std
    nuklear
)

file(GLOB_RECURSE shaders "src/*.glsl")
compile_shaders(sandbox shaders)

add_executable(sandboxMain WIN32 main.c)
target_link_libraries(sandboxMain sandbox sokolImpl)