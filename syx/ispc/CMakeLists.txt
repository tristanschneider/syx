# Add a prebuild step to the given lib which compiles the list of files
function(ispc_add_prebuild_target targetName files)
    # Output relative to whoever called this
    set(outdir out_ispc)

    # This is passed as a variable, unwrap it
    set(files ${${files}})

    # Create a unity file that includes all the other file, this will be the input to the compiler
    set(unityBaseName "unity.ispcgen")
    set(unityFile "${outdir}/${unityBaseName}")
    file(WRITE ${unityFile})
    foreach(file ${files})
        file(APPEND ${unityFile} "#include \"${file}\"\n")
    endforeach()

    # Create a script that calls the compiler on the created unity file
    set(compileScript "${outdir}/compile_ispc.bat")
    set(fullOutDir ${outdir})
    cmake_path(ABSOLUTE_PATH fullOutDir)
    set(outHeader "${fullOutDir}/unity.h")
    set(outFile "${fullOutDir}/unity.obj")
    file(WRITE ${compileScript}
        "cd \"${fullOutDir}\"\n"
        "echo \"Compiling ispc for ${targetName}...\"\n"
        "call ispc \"${unityBaseName}\" -o \"${outFile}\" -h \"${outHeader}\"\n"
    )

    cmake_path(ABSOLUTE_PATH compileScript)
    # Execute once now to generate starting state of output files to add to the project
    execute_process(COMMAND ${compileScript})

    # Add a custom command that executes the created script
    add_custom_command(OUTPUT ${outFile} ${outHeader}
        COMMAND ${compileScript}
        DEPENDS ${files}
        COMMENT "Compile ispc for target ${targetName}"
    )
endfunction()