
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

#target_compile_options(stl PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(mimas PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(fmt PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-core PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-cpp PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-c PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
#target_compile_options(spirv-cross-glsl PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})