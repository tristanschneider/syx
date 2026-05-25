add_library(nuklear STATIC
    NuklearImpl.c
    Nuklear/nuklear.h
)
target_link_libraries(nuklear PRIVATE sokol)

target_include_directories(nuklear PUBLIC .)
