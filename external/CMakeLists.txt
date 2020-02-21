add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/mimas")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/fmt")

target_compile_options(mimas PUBLIC "-w")
target_compile_options(fmt PUBLIC "-w")

set(PHOBOS_INCLUDE_DIRECTORIES ${PHOBOS_INCLUDE_DIRECTORIES}
    "${CMAKE_CURRENT_SOURCE_DIR}" 
    "${CMAKE_CURRENT_SOURCE_DIR}/mimas/public" 
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/examples"
    "${CMAKE_CURRENT_SOURCE_DIR}/glm/"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmt/include"
    PARENT_SCOPE   
)

set(PHOBOS_LINK_LIBRARIES ${PHOBOS_LINK_LIBRARIES} 
    mimas
    fmt
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
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_mimas.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_vulkan.cpp"
    PARENT_SCOPE
)

set(PHOBOS_TEST_APP_INCLUDE_DIRS
    ${PHOBOS_TEST_APP_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/examples"
    "${CMAKE_CURRENT_SOURCE_DIR}/glm/"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmt/include"
    PARENT_SCOPE
)
