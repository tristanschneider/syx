# Gather files matching pattern into source_files
file(GLOB_RECURSE source_files *.cpp *.h)

add_library(engine_shared STATIC ${source_files})

# Include this directory to anyone that includes this cmakelists
target_include_directories(engine_shared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Include directories for this
target_include_directories(engine_shared PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(engine_shared PRIVATE Precompile.h)


# Link dependencies
target_link_libraries(engine_shared PUBLIC poa)
target_link_libraries(engine_shared PUBLIC ecx)
target_link_libraries(engine_shared PUBLIC physics)
target_link_libraries(engine_shared PUBLIC imgui)
# Glew should ultimately be extracted to a seperate graphics library
target_link_libraries(engine_shared PRIVATE glew)
target_link_libraries(engine_shared PUBLIC lua)

# Asset Importer
vcpkg_install("assimp")
find_package(assimp CONFIG REQUIRED)
target_link_libraries(engine_shared PRIVATE assimp::assimp)

# This is so inconvenient, there must be a better way
add_library(assimp_lib SHARED IMPORTED GLOBAL)
set_target_properties(assimp_lib PROPERTIES
    IMPORTED_LOCATION "${VCPKG_BASE}/installed/${VCPKG_ARCH}/bin/assimp-vc142-mt.dll"
    IMPORTED_LOCATION_DEBUG "${VCPKG_BASE}/installed/${VCPKG_ARCH}/debug/bin/assimp-vc142-mtd.dll"
    IMPORTED_LOCATION_RELWITHDEBINFO "${VCPKG_BASE}/installed/${VCPKG_ARCH}/debug/bin/assimp-vc142-mtd.dll"
    IMPORTED_IMPLIB "${VCPKG_BASE}/installed/${VCPKG_ARCH}/lib/assimp-vc142-mt.lib"
    IMPORTED_IMPLIB_DEBUG "${VCPKG_BASE}/installed/${VCPKG_ARCH}/debug/lib/assimp-vc142-mtd.lib"
    )

add_library(pugixml_lib SHARED IMPORTED GLOBAL)
set_target_properties(pugixml_lib PROPERTIES
    IMPORTED_LOCATION "${VCPKG_BASE}/installed/${VCPKG_ARCH}/bin/pugixml.dll"
    IMPORTED_IMPLIB "${VCPKG_BASE}/installed/${VCPKG_ARCH}/lib/pugixml.lib"
    )

add_library(zlib_lib SHARED IMPORTED GLOBAL)
set_target_properties(zlib_lib PROPERTIES
    IMPORTED_LOCATION "${VCPKG_BASE}/installed/${VCPKG_ARCH}/bin/zlib1.dll"
    IMPORTED_LOCATION_DEBUG "${VCPKG_BASE}/installed/${VCPKG_ARCH}/debug/bin/zlibd1.dll"
    IMPORTED_LOCATION_RELWITHDEBINFO "${VCPKG_BASE}/installed/${VCPKG_ARCH}/debug/bin/zlibd1.dll"
    IMPORTED_IMPLIB "${VCPKG_BASE}/installed/${VCPKG_ARCH}/lib/zlibd.lib"
    IMPORTED_IMPLIB_DEBUG "${VCPKG_BASE}/installed/${VCPKG_ARCH}/debug/lib/zlib.lib"
    IMPORTED_IMPLIB_RELWITHDEBINFO "${VCPKG_BASE}/installed/${VCPKG_ARCH}/debug/lib/zlib.lib"
    )


# Intended to be called by the executable that imports this directly or indirectly
function(copy_data_to_target target)
    # Build file list
    set(data_dir "${CMAKE_SOURCE_DIR}/data/")

    # Copy assimp dll
    add_custom_command(TARGET ${target} COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:assimp_lib>
        $<TARGET_FILE:pugixml_lib>
        $<TARGET_FILE:zlib_lib>
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>
    )

    # Copy list to target
    add_custom_command(TARGET ${target} COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${data_dir}"
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIGURATION>/data"
    )
endfunction()