add_library(table INTERFACE)

file(GLOB_RECURSE headers *.h)
target_sources(table INTERFACE ${headers})

# Include this directory to anyone that includes this cmakelists
target_include_directories(table INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE headers *.h)
target_sources(table INTERFACE ${headers})
