# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files
    sokol/*.h
    sokol/util/*.h
)

add_library(sokol INTERFACE)

target_compile_definitions(sokol INTERFACE SOKOL_GLCORE=1)

# Include this directory to anyone that includes this cmakelists
target_include_directories(sokol INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/sokol
)

target_sources(sokol INTERFACE ${source_files})

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

file(GLOB_RECURSE impl_files
    src/*.cpp
    src/*.h
)

add_library(sokolImpl STATIC ${impl_files})

target_sources(sokolImpl PRIVATE ${impl_files})

target_include_directories(sokolImpl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(sokolImpl PUBLIC sokol)