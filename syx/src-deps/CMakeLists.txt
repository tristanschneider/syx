# Currently no alternative to glew but call it out as a platform dependency
if(WIN32)
    add_subdirectory(glew)
endif()
add_subdirectory(imgui)
