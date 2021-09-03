include(FetchContent)

cmake_policy(SET CMP0077 NEW)
# set(SPIRV_CROSS_STATIC ON)
# set(SPIRV_CROSS_CLI OFF)
# add_subdirectory("SPIRV-Cross")

set(BUILD_SHARED_LIBS OFF)

find_package(Vulkan REQUIRED)

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
FetchContent_Declare(
    plib
    GIT_REPOSITORY https://github.com/NotAPenguin0/plib
)

# Use custom fork of VMA that exports a cmake target
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/NotAPenguin0/VulkanMemoryAllocator
)

FetchContent_Declare(
    spirv_cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross
)

FetchContent_MakeAvailable(fmt)
FetchContent_MakeAvailable(plib)
FetchContent_MakeAvailable(vma)
FetchContent_MakeAvailable(spirv_cross)

set(SPIRV_CROSS_STATIC ON)
set(SPIRV_CROSS_CLI OFF)

target_link_libraries(fmt INTERFACE phobos-dependency)
target_link_libraries(vma INTERFACE phobos-dependency)
#target_link_libraries(spirv-cross-core INTERFACE phobos-dependency)
#target_link_libraries(spirv-cross-c INTERFACE phobos-dependency)
#target_link_libraries(spirv-cross-cpp INTERFACE phobos-dependency)
#target_link_libraries(spirv-cross-glsl INTERFACE phobos-dependency)
#target_link_libraries(plib INTERFACE phobos-dependency)
target_link_libraries(Phobos PUBLIC Vulkan::Vulkan fmt vma spirv-cross-core)
target_link_libraries(Phobos PRIVATE plib)
target_include_directories(Phobos PUBLIC ${plib_SOURCE_DIR}/include Vulkan::Vulkan)
target_include_directories(Phobos PRIVATE ${spirv_cross_SOURCE_DIR})

#target_compile_options(stl PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(mimas PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(fmt PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-core PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-cpp PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-c PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-glsl PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})