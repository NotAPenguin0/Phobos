cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)

project(Phobos CXX)

set(PHOBOS_SOURCES "")
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

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external")

add_library(Phobos STATIC ${PHOBOS_SOURCES})

target_include_directories(Phobos PUBLIC ${PHOBOS_INCLUDE_DIRECTORIES})
target_link_libraries(Phobos PUBLIC ${PHOBOS_LINK_LIBRARIES})

# Test Application

option(PHOBOS_ENABLE_TEST_APP CACHE OFF)

if (PHOBOS_ENABLE_TEST_APP)
    set(PHOBOS_TEST_APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_app.cpp")
    add_executable(App ${PHOBOS_TEST_APP_SOURCES})
    target_link_libraries(App PUBLIC Phobos)
    target_include_directories(App PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" "${Vulkan_INCLUDE_DIR}")
endif(PHOBOS_ENABLE_TEST_APP)