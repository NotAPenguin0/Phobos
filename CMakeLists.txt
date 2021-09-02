cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 20)

project(Phobos)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(PHOBOS_INCLUDE_DIRECTORIES ${PHOBOS_INCLUDE_DIRECTORIES}
    ""
)
set(PHOBOS_LINK_LIBRARIES ${PHOBOS_LINK_LIBRARIES}
    ""
) 

set(is_root_project OFF)	# indicate if this is the top-level project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(is_root_project ON)
  message(STATUS "${PROJECT_NAME} is top level")
endif()

OPTION(PHOBOS_ENABLE_RAY_TRACING "Enable building the ray tracing functionality")
OPTION(PHOBOS_ENABLE_TEST_APP "Enable building a test application" ${is_root_project})
OPTION(PHOBOS_NO_FIXED_PIPELINE  "Disable the fixed pipeline functionality")

add_library(Phobos STATIC ${PHOBOS_SOURCES})
if (${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    target_compile_options(Phobos PRIVATE 
        "-Wall" "-Wextra" "-Werror" "-Wno-reorder" "-Wno-unused-parameter" "-Wno-deprecated-declarations"
        "-Wno-c++98-compat-pedantic"
        "-Wno-enum-compare")
endif() 
 
if (WIN32)
    target_compile_definitions(Phobos PRIVATE WIN32_LEAN_AND_MEAN)
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src") 
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external")

target_include_directories(Phobos PUBLIC "include/")

# Test Application

if (${PHOBOS_ENABLE_TEST_APP})
    add_subdirectory("test")
endif()

if (${PHOBOS_ENABLE_RAY_TRACING})
    target_compile_definitions(Phobos PUBLIC PHOBOS_ENABLE_RAY_TRACING)
endif()