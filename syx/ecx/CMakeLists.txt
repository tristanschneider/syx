add_library(ecx INTERFACE)

file(GLOB_RECURSE headers *.h)
target_sources(ecx INTERFACE ${headers})

# Include this directory to anyone that includes this cmakelists
target_include_directories(ecx INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE headers *.h)
target_sources(ecx INTERFACE ${headers})
