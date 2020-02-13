add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/glfw")

target_compile_options(glfw PUBLIC "-w")

set(PHOBOS_INCLUDE_DIRECTORIES ${PHOBOS_INCLUDE_DIRECTORIES}
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/glfw/include" 
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/examples"
    PARENT_SCOPE   
)

set(PHOBOS_LINK_LIBRARIES ${PHOBOS_LINK_LIBRARIES} 
    glfw
    PARENT_SCOPE
)

# Add imgui
set(PHOBOS_TEST_APP_SOURCES
    ${PHOBOS_TEST_APP_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
    # GLFW + Vulkan
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/examples/imgui_impl_glfw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/examples/imgui_impl_vulkan.cpp"
    PARENT_SCOPE
)

set(PHOBOS_TEST_APP_INCLUDE_DIRS
    ${PHOBOS_TEST_APP_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/examples"
    PARENT_SCOPE
)
