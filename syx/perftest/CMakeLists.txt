file(GLOB_RECURSE source_files *.cpp *.h)

add_executable(perftest ${source_files})

target_link_libraries(perftest PRIVATE poa)

target_include_directories(perftest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})