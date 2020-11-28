cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)

project(Phobos)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Vulkan REQUIRED)

set(PHOBOS_INCLUDE_DIRECTORIES ${PHOBOS_INCLUDE_DIRECTORIES}
    ""
)
set(PHOBOS_LINK_LIBRARIES ${PHOBOS_LINK_LIBRARIES}
    ""
) 

OPTION(PHOBOS_ENABLE_TEST_APP CACHE OFF)
OPTION(PHOBOS_NO_FIXED_PIPELINE CACHE OFF)

add_library(Phobos STATIC ${PHOBOS_SOURCES})
if (${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    target_compile_options(Phobos PRIVATE 
        "-Wall" "-Wextra" "-Werror" "-Wno-reorder" "-Wno-unused-parameter" "-Wno-deprecated-declarations"
        "-Wno-enum-compare")
endif() 
 
if (WIN32)
    target_compile_definitions(Phobos PRIVATE WIN32_LEAN_AND_MEAN)
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src") 
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external")

#target_compile_definitions(Phobos PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_include_directories(Phobos PUBLIC Vulkan::Vulkan "include/")
target_link_libraries(Phobos PUBLIC Vulkan::Vulkan)

#if (NOT ${PHOBOS_NO_FIXED_PIPELINE})
#    file(GLOB PHOBOS_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/data/shaders/*")
#    foreach(SHADER ${PHOBOS_SHADERS}) 
#        get_filename_component(SHADER_FNAME ${SHADER} NAME)
#        add_custom_command( 
#            TARGET Phobos
#            COMMAND echo Compiling shader ${SHADER_FNAME} && 
#                    glslc ${SHADER} "-o${CMAKE_BINARY_DIR}/data/shaders/${SHADER_FNAME}.spv"
#        )
#    endforeach()
#endif()

# Test Application

if (PHOBOS_ENABLE_TEST_APP)
    add_subdirectory("test")
endif()

#if (PHOBOS_ENABLE_TEST_APP)
#    function(add_test_app EXEC_NAME SRC_FILES)
#        add_executable(${EXEC_NAME} "imgui_style.cpp" "test_app_framework.cpp" "external/imgui/imgui.cpp" "external/imgui/imgui_draw.cpp" 
#            "external/imgui/imgui_impl_mimas.cpp" "external/imgui/imgui_impl_phobos.cpp" "external/imgui/imgui_widgets.cpp"
#            ${SRC_FILES})
#        target_link_libraries(${EXEC_NAME} PUBLIC Phobos)
#        target_include_directories(${EXEC_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${Vulkan_INCLUDE_DIR}" ${PHOBOS_TEST_APP_INCLUDE_DIRS})
#    endfunction()

#    add_test_app(SimpleExample "simple_example.cpp")
#    add_test_app(DeferredExample "deferred_example.cpp")
#    add_test_app(DragonScene "dragon_scene.cpp")
#endif()