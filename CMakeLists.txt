cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)

project(Phobos)

set(PHOBOS_SOURCES "" "src/util/vma_implementation.cpp")
set(PHOBOS_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(PHOBOS_LINK_LIBRARIES "")

set(Vulkan_LIBRARY CACHE STRING "Vulkan library path")
set(Vulkan_INCLUDE_DIR CACHE STRING "Vulkan include directory")

set(PHOBOS_INCLUDE_DIRECTORIES ${PHOBOS_INCLUDE_DIRECTORIES}
    ${Vulkan_INCLUDE_DIR}
)
set(PHOBOS_LINK_LIBRARIES ${PHOBOS_LINK_LIBRARIES}
    ${Vulkan_LIBRARY}
) 

OPTION(PHOBOS_ENABLE_TEST_APP CACHE OFF)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src") 
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external")

  
add_library(Phobos STATIC ${PHOBOS_SOURCES})
if (CMAKE_CXX_COMPILER_ID MATCHES Clang)
#    target_compile_options(Phobos PRIVATE 
#        "-Wall" "-Wextra" "-Werror" "-Wno-reorder" "-Wno-unused-parameter" "-Wno-deprecated-declarations"
#        "-Wno-enum-compare")
    target_compile_options(Phobos PRIVATE "-w")  
endif() 
 
if (WIN32)
    target_compile_definitions(Phobos PRIVATE -DWIN32_LEAN_AND_MEAN)
endif()

target_include_directories(Phobos PUBLIC ${PHOBOS_INCLUDE_DIRECTORIES})
target_link_libraries(Phobos PUBLIC ${PHOBOS_LINK_LIBRARIES})

file(GLOB PHOBOS_SHADERS "${CMAKE_CURRENT_SOURCE_DIR}/data/shaders/*")
foreach(SHADER ${PHOBOS_SHADERS}) 
    get_filename_component(SHADER_FNAME ${SHADER} NAME)
    add_custom_command( 
        TARGET Phobos
        COMMAND echo Compiling shader ${SHADER_FNAME} && 
                glslc ${SHADER} "-o${CMAKE_BINARY_DIR}/data/shaders/${SHADER_FNAME}.spv"
    )
endforeach()

# Test Application

if (PHOBOS_ENABLE_TEST_APP)
    add_executable(SimpleExample "imgui_style.cpp" "test_app_framework.cpp" "simple_example.cpp")
    target_link_libraries(SimpleExample PUBLIC Phobos)
    target_include_directories(SimpleExample PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${Vulkan_INCLUDE_DIR}" ${PHOBOS_TEST_APP_INCLUDE_DIRS})

    add_executable(DeferredExample "imgui_style.cpp" "test_app_framework.cpp" "deferred_example.cpp")
    target_link_libraries(DeferredExample PUBLIC Phobos)
    target_include_directories(DeferredExample PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${Vulkan_INCLUDE_DIR}" ${PHOBOS_TEST_APP_INCLUDE_DIRS})
endif()