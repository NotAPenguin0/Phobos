add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/glfw")

target_compile_options(glfw PUBLIC "-w")

set(PHOBOS_INCLUDE_DIRECTORIES ${PHOBOS_INCLUDE_DIRECTORIES}
    "${CMAKE_CURRENT_SOURCE_DIR}/glfw/include" 
    PARENT_SCOPE   
)

set(PHOBOS_LINK_LIBRARIES ${PHOBOS_LINK_LIBRARIES} 
    glfw
    PARENT_SCOPE
)