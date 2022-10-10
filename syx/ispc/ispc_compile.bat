echo off

:main
    if -%1-==-- goto usage

    set outdir=%1
    shift

    echo "output to %outdir%"

    for %%i in (%*) do (
        :: Remove the extension
        if not %%i==%outdir% call :compile %outdir% %%~dpi %%~ni %%~xi
    )
goto end

:compile
    set outdir=%1
    set basePath=%2
    set fileName=%3
    set extension=%4
    set fileWithExtension=""
    set infile="%2%3%4"
    set outBase=%1/%3
    set outfile="%outBase%.obj"
    set outheader="%outBase%.h"

    echo "Compiling %fileName%..."
    call ispc %infile% -o %outfile% -h %outheader%
goto :end

:usage
    echo "usage: ispc_compile outDir <file list>"
:end