include(FetchContent)

FetchContent_Declare(glfw
	GIT_REPOSITORY https://github.com/glfw/glfw
)

set(GLFW_BUILD_DOCS OFF)

FetchContent_MakeAvailable(glfw)

#target_link_libraries(glfw INTERFACE phobos-dependency)

add_executable(TestApp)
target_link_libraries(TestApp PRIVATE Phobos glfw)
target_sources(TestApp PRIVATE "main.cpp")

set(GLSLC_DIR "" CACHE STRING "glslc binary directory, or empty if in path")

file(GLOB SHADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/data/shaders/)
set(SHADER_OUTPUT_FILES "")
foreach(SHADER ${SHADER_SOURCES})
	get_filename_component(SHADER_FNAME ${SHADER} NAME)
    set(SHADER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/data/shaders/${SHADER_FNAME}.spv)
    list(APPEND SHADER_OUTPUT_FILES ${SHADER_OUTPUT})
	add_custom_command(OUTPUT ${SHADER_OUTPUT}
        COMMAND ${GLSLC_DIR}/glslc ${SHADER} "-o${SHADER_OUTPUT}"
        DEPENDS ${SHADER}
    )
endforeach()

add_custom_target(shaders
    ALL DEPENDS ${SHADER_OUTPUT_FILES}
    COMMENT "Building shaders"
)

add_dependencies(TestApp shaders)
