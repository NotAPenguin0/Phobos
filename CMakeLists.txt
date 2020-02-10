cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)

project(Phobos CXX)

set(PHOBOS_SOURCES "")
set(PHOBOS_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(PHOBOS_LINK_LIBRARIES "")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(${PROJECT_NAME} STATIC ${PHOBOS_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${PHOBOS_INCLUDE_DIRECTORIES})
target_link_libraries(${PROJECT_NAME} PUBLIC ${PHOBOS_LINK_LIBRARIES})

# Test Application

option(PHOBOS_ENABLE_TEST_APP CACHE OFF)

if (PHOBOS_ENABLE_TEST_APP)
    set(PHOBOS_TEST_APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/test_app.cpp")
    add_executable(App ${PHOBOS_TEST_APP_SOURCES})
    target_link_libraries(App PRIVATE Phobos)
    target_include_directories(App PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif(PHOBOS_ENABLE_TEST_APP)