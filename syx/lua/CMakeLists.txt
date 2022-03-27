# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.c *.h *.hpp)

file(GLOB_RECURSE exclude_files *luac.c)
list(REMOVE_ITEM source_files ${exclude_files})

add_library(lua STATIC ${source_files})

target_include_directories(lua INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    get_target_property(props lua COMPILE_OPTIONS)
    set_target_properties(lua PROPERTIES COMPILE_OPTIONS
     # Conversion from int to X possible loss of data
     # unreachable code
     # cast runcates constant value
     "${props};/wd4244;/wd4702;/wd4310"
    )
endif()
