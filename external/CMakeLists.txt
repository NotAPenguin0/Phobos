add_subdirectory("mimas")
add_subdirectory("fmt")
add_subdirectory("stl")
cmake_policy(SET CMP0077 NEW)
set(SPIRV_CROSS_STATIC ON)
set(SPIRV_CROSS_CLI OFF)
add_subdirectory("SPIRV-Cross")
set(BUILD_SHARED_LIBS OFF)


set(PHOBOS_DEPENDENCY_COMPILE_OPTIONS "")

if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" OR ${CMAKE_CXX_COMPILER_ID} MATCHES "gcc") # clang or gcc
    set(PHOBOS_DEPENDENCY_COMPILE_OPTIONS "-w")
    
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang" AND MSVC) # clang-cl
    set(PHOBOS_DEPENDENCY_COMPILE_OPTIONS "-w")
elseif(MSVC) # MSVC
    set(PHOBOS_DEPENDENCY_COMPILE_OPTIONS "/w")
endif()

target_compile_options(stl PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
target_compile_options(mimas PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
target_compile_options(fmt PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
target_compile_options(spirv-cross-core PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
target_compile_options(spirv-cross-cpp PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
target_compile_options(spirv-cross-c PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})
target_compile_options(spirv-cross-glsl PRIVATE ${PHOBOS_DEPENDENCY_COMPILE_OPTIONS})


set(PHOBOS_INCLUDE_DIRECTORIES ${PHOBOS_INCLUDE_DIRECTORIES}
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/mimas/public" 
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
    PARENT_SCOPE
)

set(PHOBOS_TEST_APP_INCLUDE_DIRS
    ${PHOBOS_TEST_APP_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/glm/"
    "${CMAKE_CURRENT_SOURCE_DIR}/fmt/include"
    PARENT_SCOPE
)
