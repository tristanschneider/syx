add_library(table STATIC)

file(GLOB_RECURSE headers *.h)
target_sources(table PUBLIC ${headers})

# Include this directory to anyone that includes this cmakelists
target_include_directories(table PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE source *.cpp)
target_sources(table PRIVATE ${headers} ${source})
