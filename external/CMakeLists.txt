add_subdirectory("mimas")
add_subdirectory("fmt")
add_subdirectory("stl")
set(SPIRV_CROSS_STATIC ON)
set(SPIRV_CROSS_CLI OFF)
add_subdirectory("SPIRV-Cross")
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(BUILD_SHARED_LIBS OFF)
set(ASSIMP_BUILD_OBJ_IMPORTER ON)
set(ASSIMP_BUILD_FBX_IMPORTER ON)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
set(ASSIMP_BUILD_TESTS OFF)
add_subdirectory("assimp")

target_compile_options(stl PRIVATE "-w")
target_compile_options(mimas PRIVATE "-w")
target_compile_options(fmt PRIVATE "-w")
target_compile_options(spirv-cross-core PRIVATE "-w")
target_compile_options(spirv-cross-cpp PRIVATE "-w")
target_compile_options(spirv-cross-c PRIVATE "-w")
target_compile_options(spirv-cross-glsl PRIVATE "-w")
target_compile_options(assimp PRIVATE "-w")

set(PHOBOS_INCLUDE_DIRECTORIES ${PHOBOS_INCLUDE_DIRECTORIES}
    "${CMAKE_CURRENT_SOURCE_DIR}" 
    "${CMAKE_CURRENT_SOURCE_DIR}/mimas/public" 
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/examples"
    "${CMAKE_CURRENT_SOURCE_DIR}/glm/"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmt/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/stl/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Cross/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/VMA/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/assimp/include"
    PARENT_SCOPE   
)

set(PHOBOS_LINK_LIBRARIES ${PHOBOS_LINK_LIBRARIES} 
    mimas
    fmt
    stl
    spirv-cross-core
    assimp
    PARENT_SCOPE
)

# Add imgui
set(PHOBOS_SOURCES
    ${PHOBOS_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
    # Mimas + Phobos! (All my own backends! :D)
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_mimas.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_impl_phobos.cpp"
    PARENT_SCOPE
)

set(PHOBOS_TEST_APP_INCLUDE_DIRS
    ${PHOBOS_TEST_APP_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/imgui/examples"
    "${CMAKE_CURRENT_SOURCE_DIR}/glm/"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmt/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/assimp/include"
    PARENT_SCOPE
)
