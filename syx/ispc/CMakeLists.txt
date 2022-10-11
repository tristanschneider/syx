# Add a prebuild step to the given lib which compiles the list of files
function(ispc_add_prebuild_target targetName files)
    # Output relative to whoever called this
    set(outdir out_ispc)

    # This is passed as a variable, unwrap it
    set(files ${${files}})
    set(compile_script ${PROJECT_SOURCE_DIR}/ispc/ispc_compile.bat)

    # Create output directories
    foreach(file ${files})
        # Make the input file relative to this
        cmake_path(RELATIVE_PATH file)
        cmake_path(SET outObj "")
        cmake_path(APPEND outObj ${outdir} ${file})
        cmake_path(SET outDirBase NORMALIZE ${outObj})
        cmake_path(REMOVE_FILENAME outDirBase)
        message("create directory ${outDirBase}")
        #file(MAKE_DIRECTORY ${outDirBase})
    endforeach()

    # Execute once now to generate starting state of output files to add to the project
    # This also clears the output directory. I wanted to do as much of the file operations in cmake as possible
    # But any file commands execute in a different phase of the pipeline that happens after executing the process
    # So clearing output for instance before the execute call would still happen after running the command, undoing the output
    message("Initial ispc compile for target ${targetName}")
    cmake_path(SET fullOutdir NORMALIZE ${CMAKE_CURRENT_SOURCE_DIR}/${outdir}/)
    execute_process(COMMAND ${compile_script} ${fullOutdir} ${files})

    file(GLOB_RECURSE outHeaders ${outdir}/*.h)
    file(GLOB_RECURSE outObjs ${outdir}/*.obj)

    message("result ${outHeaders}, ${outObjs}")

    add_custom_command(OUTPUT ${outObjs} ${outHeaders}
        COMMAND ${compile_script} ${outdir} ${files}
        DEPENDS ${files}
        COMMENT "Compile ispc for target ${targetName}"
    )
endfunction()