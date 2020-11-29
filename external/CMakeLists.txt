include(FetchContent)

cmake_policy(SET CMP0077 NEW)
# set(SPIRV_CROSS_STATIC ON)
# set(SPIRV_CROSS_CLI OFF)
# add_subdirectory("SPIRV-Cross")

set(BUILD_SHARED_LIBS OFF)

set(PHOBOS_DEPENDENCY_COMPILE_OPTIONS "")

if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "gcc") # clang or gcc
    set(PHOBOS_DEPENDENCY_COMPILE_OPTIONS "-w")
    
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" AND MSVC) # clang-cl
    set(PHOBOS_DEPENDENCY_COMPILE_OPTIONS "-w")
elseif(MSVC) # MSVC
    set(PHOBOS_DEPENDENCY_COMPILE_OPTIONS "/w")
endif()

add_library(phobos-dependency INTERFACE)
target_compile_options(phobos-dependency INTERFACE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
)

FetchContent_MakeAvailable(fmt)

target_link_libraries(fmt PRIVATE phobos-dependency)
target_link_libraries(Phobos PUBLIC fmt)

#target_compile_options(stl PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(mimas PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(fmt PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-core PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-cpp PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-c PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-glsl PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})