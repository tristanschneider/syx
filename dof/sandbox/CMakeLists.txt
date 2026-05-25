file(GLOB_RECURSE src src/*.c src/*.h)

add_library(sandbox STATIC ${src})

target_include_directories(sandbox PUBLIC src)

target_link_libraries(sandbox
PRIVATE
    sokol
    c_std
    nuklear
)

add_executable(sandboxMain WIN32 main.c)
target_link_libraries(sandboxMain sandbox sokolImpl)