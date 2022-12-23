add_library(glew SHARED IMPORTED GLOBAL)

file(GLOB_RECURSE headers *.h)
target_sources(glew INTERFACE ${headers})

# Include this directory to anyone that includes this cmakelists
target_include_directories(glew INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Specify location of imported library
# TODO: 32 bit support
set(dll_location "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/x64/glew32.dll")
set_target_properties(glew PROPERTIES IMPORTED_LOCATION "${dll_location}")
set_target_properties(glew PROPERTIES IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/Release/x64/glew32.lib")

function(copy_glew_to_target target)
    add_custom_command(TARGET ${target} COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:glew>
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>
    )
endfunction()
