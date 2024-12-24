# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files
    sokol/*.h
    sokol/util/*.h
    src/*.cpp
    src/*.h
)

add_library(sokol STATIC ${source_files})

target_compile_definitions(sokol PUBLIC SOKOL_GLCORE=1)

# Include this directory to anyone that includes this cmakelists
target_include_directories(sokol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/sokol
)

target_sources(sokol PUBLIC ${source_files})

function(compile_shaders target files)
    set(shdc "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sokol-tools-bin/bin/win32/sokol-shdc.exe")

    foreach(file IN LISTS ${files})
        string(REGEX REPLACE "[.]glsl" ".h" outfile ${file})

        add_custom_command(OUTPUT ${outfile}
            DEPENDS ${file}
            COMMAND ${shdc} --input ${file} --output ${outfile} --slang glsl430
            COMMENT "${shdc} Compiling shader ${file} to ${outfile}..."
        )
        target_sources(${target} PRIVATE ${file} ${outfile})
    endforeach()
endfunction()