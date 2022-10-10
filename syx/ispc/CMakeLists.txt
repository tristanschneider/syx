# add_subdirectory(ispc)

function(ispc_compile files)
    foreach(file files)
        cmake_path(SET input NORMALIZE file)

        #cmake_path(SET outHeader NORMALIZE file)
        cmake_path(REPLACE_EXTENSION outHeader LASTONLY ".h")

        message("Compile ${file} with header ${outHeader}")
        #set(outObj file)
    endforeach()
endfunction()

# Add a prebuild step to the given lib which compiles the list of files
function(ispc_add_prebuild_target targetName files)
    # Output relative to whoever called this
    set(outdir out_ispc)
    file(MAKE_DIRECTORY ${outdir})

    # Delete old contents if they existed already
    file(REMOVE_RECURSE ${outdir})

    # This is passed as a variable, unwrap it
    set(files ${${files}})

    # Create empty obj and header files for each of the input files
    foreach(file ${files})
        message("for file ${file}")

        # Make the input file relative to this
        cmake_path(RELATIVE_PATH file)

        # Clear this or it appends continuously across loops
        cmake_path(SET outObj "")
        cmake_path(APPEND outObj ${outdir} ${file})
        cmake_path(REPLACE_EXTENSION outObj "obj")

        cmake_path(SET outHeader NORMALIZE ${outObj})
        cmake_path(REPLACE_EXTENSION outHeader "h")

        # Create directory for file if it doesn't exist
        cmake_path(SET outDirBase NORMALIZE ${outHeader})
        cmake_path(REMOVE_FILENAME outDirBase)
        file(MAKE_DIRECTORY ${outDirBase})

        # Create the placeholder files in the output directory if they don't exist
        file(TOUCH ${outObj} ${outHeader})
        message("out obj ${outObj} out header ${outHeader}")
    endforeach()

    set(compile_script ${PROJECT_SOURCE_DIR}/ispc/ispc_compile.bat)
    add_custom_target(${targetName}
        ${compile_script} ${outdir} ${files}
    )
endfunction()