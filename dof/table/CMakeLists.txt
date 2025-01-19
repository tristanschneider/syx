add_library(table STATIC)

file(GLOB_RECURSE headers *.h)
target_sources(table PUBLIC ${headers})
target_link_libraries(table PUBLIC generics)

# Include this directory to anyone that includes this cmakelists
target_include_directories(table PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE source *.cpp *.natvis)
target_sources(table PRIVATE ${headers} ${source} ${natvis})
