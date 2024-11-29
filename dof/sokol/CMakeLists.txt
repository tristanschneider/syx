# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files
    sokol/*.h
    sokol/util/*.h
)

add_library(sokol INTERFACE)

# Include this directory to anyone that includes this cmakelists
target_include_directories(sokol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/sokol)

target_sources(sokol INTERFACE ${source_files})

function(compile_shaders target files)
    message(STATUS "Create shader compiler build step for ${files}")

    set(shdc "${CMAKE_CURRENT_SOURCE_DIR}/sokol-tools-bin/sokol-shdc.exe")
    foreach(file IN LISTS files)
        string(REGEX REPLACE "[.]glsl" ".h" outfile ${file})
        message(STATUS "in ${file} out ${outfile}")

        add_custom_command(OUTPUT ${outfile}
            COMMAND ${shdc} --input ${file} --output ${outfile} --slang glsl430
        )
    endforeach()
endfunction()